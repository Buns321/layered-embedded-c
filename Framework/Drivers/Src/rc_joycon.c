/**
 * @file rc_joycon.c
 * @brief JoyCon 蓝牙串口遥控接收与解析实现。
 * @ingroup drivers
 */

#include "rc_joycon.h"
#include "sys_critical.h"
#include "sys_math.h"
#include "sys_snapshot.h"
#include <stddef.h>
#include <string.h>

#define JOYCON_FIELD_COUNT 5U
#define JOYCON_VALUE_COUNT 4U
#define JOYCON_VALUE_LIMIT 100.0f

static bool joycon_char_is_space(char ch) { return (ch == ' ') || (ch == '\t'); }

static void joycon_reset_frame(rc_joycon_t *rc) {
  rc->frame_length = 0U;
  rc->frame_overflow = false;
  rc->frame_active = false;
}

static bool joycon_parse_value(const char *text, uint16_t length, float *out_value) {
  uint16_t start = 0U;
  uint16_t end = length;

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

static bool joycon_parse_frame(const rc_joycon_t *rc, uint32_t now_ms,
                               rc_joycon_input_t *out_input) {
  float values[JOYCON_VALUE_COUNT] = {0};
  uint16_t cursor = 0U;

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

  Sys_Critical_Enter();
  rc->input = next_input;
  rc->parsed_frame_count++;
  Sys_Critical_Exit();

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
