/**
 * @file rc_joycon.c
 * @brief JoyCon 蓝牙串口遥控接收与解析实现。
 * @details 属于 Framework/Drivers 设备驱动层，实现无 strtod/sscanf 依赖的
 *          确定性 ASCII 浮点解析、`[j,lx,ly,rx,ry]` 帧同步、[-100, 100] 到
 *          [-1.0, 1.0] 归一化、ISR 环形缓冲重同步与跨任务临界区快照发布。
 * @ingroup Framework_Drivers
 */

#include "rc_joycon.h"
#include "sys_critical.h"
#include "sys_math.h"
#include "sys_snapshot.h"
#include <stddef.h>
#include <string.h>

/** @brief 协议帧总字段数（1 个帧标识 `j`/`J` + 4 个摇杆数值）。 */
#define JOYCON_FIELD_COUNT 5U
/** @brief 摇杆数值通道数量（left_x, left_y, right_x, right_y）。 */
#define JOYCON_VALUE_COUNT 4U
/** @brief 协议原始摇杆数值绝对值上限（对应 ±100 映射到 ±1.0）。 */
#define JOYCON_VALUE_LIMIT 100.0f

/**
 * @brief 判断字符是否为空白分隔符（空格或制表符）。
 * @param[in] ch 待检查字符。
 * @return 是否为空白字符。
 */
static bool joycon_char_is_space(char ch) { return (ch == ' ') || (ch == '\t'); }

/**
 * @brief 复位当前 ASCII 帧组装状态机。
 * @param[in,out] rc JoyCon 对象指针。
 */
static void joycon_reset_frame(rc_joycon_t *rc) {
  rc->frame_length = 0U;
  rc->frame_overflow = false;
  rc->frame_active = false;
}

/**
 * @brief 解析单个带可选符号与小数的 ASCII 数值字段，并校验绝对值不超过 100。
 * @param[in]  text      字段首地址（非 NULL 结束，按 length 界定）。
 * @param[in]  length    字段字符长度。
 * @param[out] out_value 解析后的浮点数输出指针（范围 [-100.0, 100.0]）。
 * @return 解析与范围校验是否成功。
 */
static bool joycon_parse_value(const char *text, uint16_t length, float *out_value) {
  uint16_t start = 0U;
  uint16_t end = length;

  // 裁剪字段前后空白字符
  while (start < end && joycon_char_is_space(text[start])) {
    start++;
  }
  while (end > start && joycon_char_is_space(text[end - 1U])) {
    end--;
  }
  if (start == end) {
    return false;
  }

  bool negative = false;
  if (text[start] == '+' || text[start] == '-') {
    negative = (text[start] == '-');
    start++;
  }
  if (start == end) {
    return false;
  }

  float magnitude = 0.0f;
  float fraction_scale = 0.1f;
  bool has_digit = false;
  bool has_dot = false;

  for (uint16_t i = start; i < end; ++i) {
    char cur = text[i];
    if (cur == '.') {
      if (has_dot) {
        return false;
      }
      has_dot = true;
      continue;
    }
    if (cur < '0' || cur > '9') {
      return false;
    }

    has_digit = true;
    float digit = (float)(cur - '0');
    if (has_dot) {
      magnitude += digit * fraction_scale;
      fraction_scale *= 0.1f;
    } else {
      magnitude = (magnitude * 10.0f) + digit;
    }
    if (!SYS_ISFINITE(magnitude)) {
      return false;
    }
  }

  if (!has_digit || magnitude > JOYCON_VALUE_LIMIT) {
    return false;
  }

  *out_value = negative ? -magnitude : magnitude;
  return true;
}

/**
 * @brief 按逗号切分并解析完整 `[j,lx,ly,rx,ry]` 载荷，生成归一化输入快照。
 * @param[in]  rc        JoyCon 对象指针。
 * @param[in]  now_ms    当前系统单调时间戳（单位：ms）。
 * @param[out] out_input 输出归一化快照指针。
 * @return 帧格式与各字段数值是否全部合法。
 */
static bool joycon_parse_frame(const rc_joycon_t *rc, uint32_t now_ms,
                               rc_joycon_input_t *out_input) {
  float values[JOYCON_VALUE_COUNT] = {0};
  uint16_t cursor = 0U;

  // 严格匹配 5 个逗号分隔字段：field0 必须为单字符 'j'/'J'，field1..4 为 [-100, 100] 数值
  for (uint16_t field = 0U; field < JOYCON_FIELD_COUNT; ++field) {
    uint16_t field_start = cursor;
    while (cursor < rc->frame_length && rc->frame_text[cursor] != ',') {
      cursor++;
    }

    uint16_t field_len = (uint16_t)(cursor - field_start);
    if (field == 0U) {
      if (field_len != 1U ||
          (rc->frame_text[field_start] != 'j' && rc->frame_text[field_start] != 'J')) {
        return false;
      }
    } else if (!joycon_parse_value(&rc->frame_text[field_start], field_len, &values[field - 1U])) {
      return false;
    }

    bool has_sep = (cursor < rc->frame_length) && (rc->frame_text[cursor] == ',');
    if ((field + 1U) < JOYCON_FIELD_COUNT) {
      if (!has_sep) {
        return false;
      }
      cursor++;
    } else if (has_sep || cursor != rc->frame_length) {
      return false;
    }
  }

  *out_input = (rc_joycon_input_t){
      .left_x_norm = values[0] / JOYCON_VALUE_LIMIT,
      .left_y_norm = values[1] / JOYCON_VALUE_LIMIT,
      .right_x_norm = values[2] / JOYCON_VALUE_LIMIT,
      .right_y_norm = values[3] / JOYCON_VALUE_LIMIT,
      .time_ms = now_ms,
      .sequence = rc->input.sequence + 1U,
  };
  return true;
}

/**
 * @brief 校验、发布当前组装完成的帧并在有效时喂狗。
 * @param[in,out] rc     JoyCon 对象指针。
 * @param[in]     now_ms 当前系统单调时间戳（单位：ms）。
 */
static void joycon_publish_frame(rc_joycon_t *rc, uint32_t now_ms) {
  if (rc->frame_overflow) {
    rc->invalid_frame_count++;
    return;
  }

  rc_joycon_input_t next_input = {0};
  if (!joycon_parse_frame(rc, now_ms, &next_input)) {
    rc->invalid_frame_count++;
    return;
  }

  // 临界区原子写入四轴快照与帧计数，防止控制任务读到跨帧撕裂的摇杆值
  Sys_Critical_Enter();
  rc->input = next_input;
  rc->parsed_frame_count++;
  Sys_Critical_Exit();

  // 仅在收到合法完整帧时喂狗，防止串口乱码或半帧阻塞离线检测
  if (rc->daemon != NULL) {
    Sys_Daemon_Feed(rc->daemon, now_ms);
  }
}

bsp_status_t RC_JoyCon_Init(rc_joycon_t *rc, const rc_joycon_cfg_t *cfg) {
  if (rc == NULL || cfg == NULL || cfg->huart == NULL) {
    return BSP_ERR_INVALID_ARG;
  }

  bsp_uart_rx_mode_t rx_mode =
      (cfg->rx_mode == BSP_UART_RX_NONE) ? BSP_UART_RX_IT_BYTE : cfg->rx_mode;
  if (rx_mode != BSP_UART_RX_IT_BYTE && rx_mode != BSP_UART_RX_DMA_IDLE) {
    return BSP_ERR_INVALID_ARG;
  }

  (void)memset(rc, 0, sizeof(*rc));

  const bsp_uart_cfg_t uart_cfg = {
      .huart = cfg->huart,
      .rx_mode = rx_mode,
      .rx_ring_buf = rc->rx_ring_buf,
      .rx_ring_size = (uint16_t)sizeof(rc->rx_ring_buf),
      .rx_dma_buf = (rx_mode == BSP_UART_RX_DMA_IDLE) ? rc->rx_dma_buf : NULL,
      .rx_dma_size = (rx_mode == BSP_UART_RX_DMA_IDLE) ? (uint16_t)sizeof(rc->rx_dma_buf) : 0U,
      .on_tx_done = NULL,
      .tx_ctx = NULL,
  };

  bsp_status_t st = BSP_UART_Init(&rc->uart, &uart_cfg);
  if (st != BSP_OK) {
    return st;
  }

  rc->daemon = cfg->daemon;
  return BSP_OK;
}

bsp_status_t RC_JoyCon_Start(rc_joycon_t *rc) {
  if (rc == NULL || !rc->uart.registered || rc->is_started) {
    return BSP_ERR_INVALID_ARG;
  }
  rc->is_started = true;
  return BSP_UART_StartRx(&rc->uart);
}

void RC_JoyCon_Process(rc_joycon_t *rc, uint32_t now_ms) {
  if (rc == NULL || !rc->is_started) {
    return;
  }

  // ISR 仅负责将字节压入环形缓冲并在溢出/帧错时置 resync 标志；任务侧负责排空与解析
  BSP_UART_EnsureRxArmed(&rc->uart);
  if (BSP_UART_TakeResync(&rc->uart)) {
    joycon_reset_frame(rc);
  }

  uint8_t byte = 0U;
  while (BSP_UART_PopByte(&rc->uart, &byte)) {
    if (BSP_UART_TakeResync(&rc->uart)) {
      joycon_reset_frame(rc);
    }
    RC_JoyCon_FeedByte(rc, byte, now_ms);
  }
}

void RC_JoyCon_FeedByte(rc_joycon_t *rc, uint8_t byte, uint32_t now_ms) {
  if (rc == NULL) {
    return;
  }

  if (!rc->frame_active) {
    if (byte == (uint8_t)'[') {
      rc->frame_active = true;
      rc->frame_length = 0U;
      rc->frame_overflow = false;
    }
    return;
  }

  // 帧内再次遇到 `[` 说明上一帧未正常闭合，记为无效帧并以新 `[` 重新同步
  if (byte == (uint8_t)'[') {
    if (rc->frame_length != 0U || rc->frame_overflow) {
      rc->invalid_frame_count++;
    }
    rc->frame_length = 0U;
    rc->frame_overflow = false;
    return;
  }

  if (byte == (uint8_t)']') {
    joycon_publish_frame(rc, now_ms);
    joycon_reset_frame(rc);
    return;
  }

  // 忽略蓝牙串口可能附带的 CR/LF 换行符
  if (byte == (uint8_t)'\r' || byte == (uint8_t)'\n') {
    return;
  }

  if (rc->frame_length >= RC_JOYCON_FRAME_CAPACITY) {
    rc->frame_overflow = true;
    return;
  }

  rc->frame_text[rc->frame_length] = (char)byte;
  rc->frame_length++;
}

bool RC_JoyCon_GetInput(const rc_joycon_t *rc, rc_joycon_input_t *out_input) {
  if (rc == NULL || out_input == NULL) {
    return false;
  }
  SYS_SNAPSHOT_READ(out_input, &rc->input);
  return out_input->sequence != 0U;
}

void RC_JoyCon_GetStatus(const rc_joycon_t *rc, rc_joycon_status_t *out_status) {
  if (rc == NULL || out_status == NULL) {
    return;
  }
  Sys_Critical_Enter();
  *out_status = (rc_joycon_status_t){
      .is_started = rc->is_started,
      .received_bytes = rc->uart.rx_bytes,
      .parsed_frames = rc->parsed_frame_count,
      .invalid_frames = rc->invalid_frame_count,
      .rx_dropped_bytes = rc->uart.rx_dropped_bytes,
      .rx_error_count = rc->uart.rx_error_count,
      .last_frame_ms = rc->input.time_ms,
      .sequence = rc->input.sequence,
  };
  Sys_Critical_Exit();
}
