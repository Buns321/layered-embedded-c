//
// 由 Buns_ 创建于 2026/10/26。
//

/**
 * @file joycon_uart.c
 * @brief JoyCon ASCII UART 命令接收设备实现。
 * @ingroup drivers_concrete
 */

#include "joycon_uart.h"
#include "system_critical.h"
#include "system_math.h"
#include <assert.h>
#include <string.h>

#define JOYCON_UART_FIELD_COUNT 5u
#define JOYCON_UART_VALUE_COUNT 4u
#define JOYCON_UART_VALUE_LIMIT 100.0f

/**
 * @brief 通过 base 操作表复制最近一次 JoyCon 输入。
 * @param joycon_h base 视图；必须指向 joycon_uart_t 的首成员，回调内反转为具体对象。
 * @param input 输出快照；调用方拥有，函数在临界区内覆盖全部字段。
 * @return 已发布快照的 sequence 非零时为 true，否则为 false。
 * @pre 两个指针非空；joycon_h 必须指向完整的具体对象，不能是独立 base 副本。
 * @post input 含最近成功解析的归一化摇杆量；返回值反映是否存在有效帧。
 * @note 私有回调，只在 task 上下文经 ops 表调用；不得在 ISR 中调用。
 */
static bool dev_joycon_uart_getinputop(
  joycon_base_t *joycon_h,
  joycon_input_t *input
);
/**
 * @brief 通过 base 操作表复制 JoyCon 运行状态。
 * @param joycon_h base 视图；必须指向 joycon_uart_t 的首成员。
 * @param status 输出缓冲区；调用方拥有，函数在临界区内完整覆盖。
 * @pre 两个指针非空；joycon_h 指向完整具体对象；调用方不得与析构竞态。
 * @post status 含启动标志、计数器和最后一帧元数据的一致快照。
 * @note 私有回调，只在 task 上下文经 ops 表调用；临界区只复制字段，不访问 UART。
 */
static void dev_joycon_uart_getstatusop(
  joycon_base_t *joycon_h,
  joycon_status_t *status
);

static const joycon_ops_t joycon_ops_uart = {
  .get_input = dev_joycon_uart_getinputop,
  .get_status = dev_joycon_uart_getstatusop,
};

/**
 * @brief 判断字符是否为帧内允许忽略的空格或制表符。
 * @param value 待判断字符；其他控制字符不视为空白。
 * @return value 为空格或制表符时返回 true，否则返回 false。
 * @note 纯函数，无状态、无硬件访问；只用于字段首尾裁剪，不处理 CR/LF（由 FeedByte 丢弃）。
 */
static bool joycon_char_is_space(char value) {
  return value == ' ' || value == '\t';
}

/**
 * @brief 清空文本组帧状态，准备接收下一帧。
 * @param joycon_h 待复位的组帧状态对象；必须由 Init 初始化，且当前任务独占其解析状态。
 * @pre joycon_h 非空；只能由持有解析状态的任务调用，不得与 FeedByte 并发。
 * @post frame_length 归零，frame_overflow 与 frame_active 置 false；已发布快照不受影响。
 * @note 不清空 rx 环形队列或计数；帧结束后由 FeedByte 调用。
 */
static void joycon_reset_frame(joycon_uart_t *joycon_h) {
  assert(joycon_h);

  joycon_h->frame_length = 0u;
  joycon_h->frame_overflow = false;
  joycon_h->frame_active = false;
}

/**
 * @brief 解析一个带可选符号和小数点的 ASCII 数值字段。
 * @param text 字段起始地址；不要求以 NUL 结尾，只读取 length 指定的字节。
 * @param length 字段字节数，不含分隔逗号；0 或仅含空白时解析失败。
 * @param value 输出数值，单位与协议一致，量程 [-100, 100]；调用方拥有。
 * @return 字段格式合法且绝对值不超过 100 时返回 true；否则返回 false 且不写 value。
 * @pre text、value 非空；text 至少在 length 字节内可读。
 * @post 成功时 value 为去空白后的带符号数值；失败时 value 保持原值。
 * @note 仅接受可选 '+'/'-'、十进制数字和至多一个小数点；不处理指数、NUL 结束或多字节字符。
 */
static bool joycon_parse_value(
  const char *text,
  uint16_t length,
  float *value
) {
  assert(text && value);

  uint16_t start = 0u;
  uint16_t end = length;

  while (start < end && joycon_char_is_space(text[start])) {
    start++;
  }
  while (end > start && joycon_char_is_space(text[end - 1u])) {
    end--;
  }

  if (start == end) {
    return false;
  }

  bool negative = false;
  if (text[start] == '+' || text[start] == '-') {
    negative = text[start] == '-';
    start++;
  }

  if (start == end) {
    return false;
  }

  float magnitude = 0.0f;
  float fraction_scale = 0.1f;
  bool has_digit = false;
  bool has_decimal_point = false;

  for (uint16_t i = start; i < end; i++) {
    char current = text[i];

    if (current == '.') {
      if (has_decimal_point) {
        return false;
      }
      has_decimal_point = true;
      continue;
    }

    if (current < '0' || current > '9') {
      return false;
    }

    has_digit = true;
    float digit = (float)(current - '0');

    if (has_decimal_point) {
      magnitude += digit * fraction_scale;
      fraction_scale *= 0.1f;
    } else {
      magnitude = magnitude * 10.0f + digit;
    }

    if (!SYS_ISFINITE(magnitude)) {
      return false;
    }
  }

  if (!has_digit || magnitude > JOYCON_UART_VALUE_LIMIT) {
    return false;
  }

  *value = negative ? -magnitude : magnitude;
  return true;
}

/**
 * @brief 解析形如 "j,lx,ly,rx,ry" 的完整 JoyCon 文本帧。
 * @param joycon_h 只读对象，提供 frame_text 与 frame_length。
 * @param now_ms 当前单调毫秒时间戳，写入输出帧的 time_ms。
 * @param input 输出帧；成功后摇杆量归一化到 [-1, 1]，sequence 递增。
 * @return 五个字段齐全、字段数正确且数值均在 [-100, 100] 时返回 true；否则返回 false。
 * @pre joycon_h、input 非空；frame_length 不超过 JOYCON_UART_FRAME_CAPACITY。
 * @post 成功时 input 全部字段有效；失败时 input 不被修改。
 * @note 首字段必须为单字符 'j' 或 'J'；不允许多余逗号、空字段或尾随内容。
 */
static bool joycon_parse_frame(
  const joycon_uart_t *joycon_h,
  uint32_t now_ms,
  joycon_input_t *input
) {
  assert(joycon_h && input);

  float values[JOYCON_UART_VALUE_COUNT] = {0};
  uint16_t cursor = 0u;

  for (uint16_t field = 0u; field < JOYCON_UART_FIELD_COUNT; field++) {
    uint16_t field_start = cursor;
    while (
      cursor < joycon_h->frame_length &&
      joycon_h->frame_text[cursor] != ','
    ) {
      cursor++;
    }

    uint16_t field_length = (uint16_t)(cursor - field_start);

    if (field == 0u) {
      if (
        field_length != 1u ||
        (joycon_h->frame_text[field_start] != 'j' &&
         joycon_h->frame_text[field_start] != 'J')
      ) {
        return false;
      }
    } else if (!joycon_parse_value(
                 &joycon_h->frame_text[field_start],
                 field_length,
                 &values[field - 1u]
               )) {
      return false;
    }

    bool has_separator =
      cursor < joycon_h->frame_length &&
      joycon_h->frame_text[cursor] == ',';

    if (field + 1u < JOYCON_UART_FIELD_COUNT) {
      if (!has_separator) {
        return false;
      }
      cursor++;
    } else if (has_separator || cursor != joycon_h->frame_length) {
      return false;
    }
  }

  *input = (joycon_input_t){
    .left_x = values[0] / JOYCON_UART_VALUE_LIMIT,
    .left_y = values[1] / JOYCON_UART_VALUE_LIMIT,
    .right_x = values[2] / JOYCON_UART_VALUE_LIMIT,
    .right_y = values[3] / JOYCON_UART_VALUE_LIMIT,
    .time_ms = now_ms,
    .sequence = joycon_h->input.sequence + 1u,
  };

  return true;
}

/**
 * @brief 校验当前组帧缓冲并原子发布有效快照。
 * @param joycon_h 已完成一帧累积的解析对象；必须由任务上下文独占，且已遇到帧结束符。
 * @param now_ms 当前单调毫秒时间戳，仅用于成功帧。
 * @pre joycon_h 非空；只在任务上下文、单写者条件下调用；组帧已遇到 ']'。
 * @post 有效帧写入 input 并递增 parsed_frame_count；溢出或格式错误只递增 invalid_frame_count。
 * @note 发布使用 System 临界区，读取方最多看到上一帧或当前完整帧，不会看到半更新字段。
 */
static void joycon_publish_frame(
  joycon_uart_t *joycon_h,
  uint32_t now_ms
) {
  assert(joycon_h);

  if (joycon_h->frame_overflow) {
    joycon_h->invalid_frame_count++;
    return;
  }

  joycon_input_t input = {0};
  if (!joycon_parse_frame(joycon_h, now_ms, &input)) {
    joycon_h->invalid_frame_count++;
    return;
  }

  Sys_Critical_Enter();
  joycon_h->input = input;
  joycon_h->parsed_frame_count++;
  Sys_Critical_Exit();
}

/**
 * @brief 重新挂接单字节中断接收，并记录失败。
 * @param joycon_h 已绑定 uart_h 的对象；rx_byte 是接收目标，挂接期间必须保持有效。
 * @pre joycon_h 与 uart_h 非空；可在任务或 UART 中断上下文调用。
 * @post Dev_UART_ReceiveIT 成功时清除 rx_rearm_pending；失败时置位并递增 rearm_error_count。
 * @note 不阻塞、不等待；UART 后端返回任何非 OK 状态都按待重挂处理。
 * @warning 中断与任务都可能调用本函数；rx_rearm_pending 为 volatile，但实现不做互斥，调用方需保证不在同一时刻并发重挂。
 */
static void joycon_rearm_receive(joycon_uart_t *joycon_h) {
  assert(joycon_h && joycon_h->uart_h);

  dev_uart_status_t status = Dev_UART_ReceiveIT(
    joycon_h->uart_h,
    &joycon_h->rx_byte,
    1u
  );

  if (status == DEV_UART_OK) {
    joycon_h->rx_rearm_pending = false;
    return;
  }

  joycon_h->rx_rearm_pending = true;
  joycon_h->rearm_error_count++;
}

/**
 * @brief UART 接收完成中断回调：入队单字节并重新挂接。
 * @param uart_h 触发事件的 UART 后端；必须等于对象保存的 uart_h。
 * @param context 注册时传入的 joycon_uart_t 指针，由 UART 后端原样回传。
 * @pre 回调和对象生命周期覆盖注册期；只在中断上下文执行。
 * @post rx_byte_count 递增；队列未满时写入一字节并推进 rx_write，满时只递增 rx_overflow_count；随后尝试重挂接收。
 * @note 不在 ISR 中解析 ASCII，也不调用临界区或阻塞 API。
 * @warning 队列实际容量为 255 字节；溢出字节被丢弃，错误仅通过计数暴露。
 */
static void joycon_rx_complete_cb(
  uart_base_t *uart_h,
  void *context
) {
  joycon_uart_t *joycon_h = (joycon_uart_t *)context;
  assert(joycon_h && uart_h == joycon_h->uart_h);
  (void)uart_h;

  joycon_h->rx_byte_count++;

  uint16_t next_write =
    (joycon_h->rx_write + 1u) & JOYCON_UART_RX_INDEX_MASK;

  if (next_write == joycon_h->rx_read) {
    joycon_h->rx_overflow_count++;
  } else {
    joycon_h->rx_buffer[joycon_h->rx_write] = joycon_h->rx_byte;
    joycon_h->rx_write = next_write;
  }

  joycon_rearm_receive(joycon_h);
}

/**
 * @brief UART 错误中断回调：计数并重新挂接接收。
 * @param uart_h 触发错误的 UART 后端；必须等于对象保存的 uart_h。
 * @param context 注册时传入的 joycon_uart_t 指针。
 * @pre 回调和对象生命周期覆盖注册期；只在中断上下文执行。
 * @post uart_error_count 递增并尝试重挂接收；组帧状态和已发布快照保持不变。
 * @note 与 WHT101 不同，本回调不请求重新同步，ASCII 解析器可从下一字节继续组帧。
 * @warning 重挂失败只通过 rearm_error_count 与状态快照暴露，本回调无法通知上层。
 */
static void joycon_uart_error_cb(
  uart_base_t *uart_h,
  void *context
) {
  joycon_uart_t *joycon_h = (joycon_uart_t *)context;
  assert(joycon_h && uart_h == joycon_h->uart_h);
  (void)uart_h;

  joycon_h->uart_error_count++;
  joycon_rearm_receive(joycon_h);
}

/**
 * @brief 初始化 JoyCon UART 设备对象并绑定静态操作表。
 * @param joycon_h 具体对象；调用方静态分配，函数会清零全部字段。
 * @param name 调试名称；只保存指针，其生命周期必须覆盖对象。
 * @param uart_h 已初始化的 UART 后端；只保存非 owning 指针，后端必须支持中断接收。
 * @pre joycon_h、name、uart_h 非空；对象未被并发使用；同一物理 UART 没有其他回调所有者。
 * @post joycon_h 全部字段清零，base 绑定 JoyCon 操作表并保存 name，uart_h 已记录；is_started 为 false。
 * @note 不注册回调、不启动接收，也不调用 UART 传输接口，因此没有硬件副作用。
 * @note 必须在任务上下文调用，且接收启动后不得重复调用。
 */
void Dev_JoyCon_UART_Init(
  joycon_uart_t *joycon_h,
  const char *name,
  uart_base_t *uart_h
) {
  assert(joycon_h && name && uart_h);

  (void)memset(joycon_h, 0, sizeof(*joycon_h));
  Dev_JoyCon_Base_Init(&joycon_h->base, name, &joycon_ops_uart);
  joycon_h->uart_h = uart_h;
}

/**
 * @brief 注册接收/错误回调并启动单字节中断接收。
 * @param joycon_h 已由 Init 初始化且尚未启动的设备对象。
 * @retval DEV_UART_OK 回调已注册且首次单字节接收挂接成功。
 * @retval DEV_UART_ERROR 首次 Dev_UART_ReceiveIT 失败；此处 is_started 仍为 true，Process 会继续重试。
 * @pre joycon_h 非空且已 Init；is_started 为 false；UART 后端可用；同一物理 UART 无其他回调所有者。
 * @post 回调写入 UART 后端（覆盖原有回调），is_started 为 true；接收处于已挂接或待重挂状态。
 * @note 注册本身不阻塞，接收完成回调在中断上下文执行；只允许在任务上下文调用一次。
 * @warning 实现不检测 UART 是否已被其他所有者占用；覆盖回调可能使其他接收者失联，且不会返回 BUSY。
 */
dev_uart_status_t Dev_JoyCon_UART_Start(joycon_uart_t *joycon_h) {
  assert(joycon_h && joycon_h->uart_h && !joycon_h->is_started);

  dev_uart_callbacks_t callbacks = {
    .rx_complete_cb = joycon_rx_complete_cb,
    .err_cb = joycon_uart_error_cb,
    .context = joycon_h,
  };

  Dev_UART_RegisterCallback(joycon_h->uart_h, &callbacks);
  joycon_h->is_started = true;
  joycon_rearm_receive(joycon_h);

  if (joycon_h->rx_rearm_pending) {
    return DEV_UART_ERROR;
  }

  return DEV_UART_OK;
}

/**
 * @brief 处理待重挂接收并解析环形队列中的全部字节。
 * @param joycon_h 已 Start 的对象；rx_read 只能由本任务推进。
 * @param now_ms 当前单调毫秒时间戳；同一轮完成的一帧使用该时间。
 * @pre joycon_h 非空且已 Start；仅由单一 UART 任务调用，不得与 FeedByte 或其他 Process 并发。
 * @post rx_read 追上 rx_write，队列字节按序送入解析器；可能发布新快照并更新计数；如有待重挂则重试。
 * @note 不等待数据，队列为空时立即返回；ISR 只写 rx_write，任务只写 rx_read。
 * @warning 同一批字节共享调用时的 now_ms，不能用于逐字节计时。
 */
void Dev_JoyCon_UART_Process(
  joycon_uart_t *joycon_h,
  uint32_t now_ms
) {
  assert(joycon_h && joycon_h->is_started);

  if (joycon_h->rx_rearm_pending) {
    joycon_rearm_receive(joycon_h);
  }

  while (joycon_h->rx_read != joycon_h->rx_write) {
    uint8_t byte = joycon_h->rx_buffer[joycon_h->rx_read];
    joycon_h->rx_read =
      (joycon_h->rx_read + 1u) & JOYCON_UART_RX_INDEX_MASK;
    Dev_JoyCon_UART_FeedByte(joycon_h, byte, now_ms);
  }
}

/**
 * @brief 向 JoyCon ASCII 状态机输入一个字节。
 * @param joycon_h 已 Init 的对象；由 Process 或主机测试直接调用。
 * @param byte 输入字节，取值 0..255；'[' 开始帧，']' 结束帧，CR/LF 被忽略。
 * @param now_ms 当前单调毫秒时间戳；仅在字节完成有效帧时作为该帧的 time_ms。
 * @pre joycon_h 非空；对象未被其他任务并发解析；不得在 ISR 中调用。
 * @post 组帧缓冲最多接收 64 字节；超长帧置溢出标志并在 ']' 时计为无效；有效帧在临界区内原子发布。
 * @note 失败不返回错误，只递增 invalid_frame_count；溢出帧丢弃且不影响上一帧快照。
 */
void Dev_JoyCon_UART_FeedByte(
  joycon_uart_t *joycon_h,
  uint8_t byte,
  uint32_t now_ms
) {
  assert(joycon_h);

  if (!joycon_h->frame_active) {
    if (byte == (uint8_t)'[') {
      joycon_h->frame_active = true;
      joycon_h->frame_length = 0u;
      joycon_h->frame_overflow = false;
    }
    return;
  }

  if (byte == (uint8_t)'[') {
    if (joycon_h->frame_length != 0u || joycon_h->frame_overflow) {
      joycon_h->invalid_frame_count++;
    }
    joycon_h->frame_length = 0u;
    joycon_h->frame_overflow = false;
    return;
  }

  if (byte == (uint8_t)']') {
    joycon_publish_frame(joycon_h, now_ms);
    joycon_reset_frame(joycon_h);
    return;
  }

  if (byte == (uint8_t)'\r' || byte == (uint8_t)'\n') {
    return;
  }

  if (joycon_h->frame_length >= JOYCON_UART_FRAME_CAPACITY) {
    joycon_h->frame_overflow = true;
    return;
  }

  joycon_h->frame_text[joycon_h->frame_length] = (char)byte;
  joycon_h->frame_length++;
}

/**
 * @brief 复制最近一帧有效 JoyCon 输入快照。
 * @param joycon_h 已 Init 的对象；允许其他任务读取。
 * @param input 输出缓冲区；调用方拥有，函数在临界区内覆盖全部字段，即使返回 false 也会写入零快照。
 * @return 快照 sequence 非零时返回 true，表示至少成功解析过一帧；false 表示尚无有效帧。
 * @pre joycon_h、input 非空；input 的生命周期覆盖调用。
 * @post input 含最近一次成功发布的归一化摇杆量、毫秒时间戳和序号。
 * @note 复制期间进入 System 临界区，不得在 ISR 中调用；临界区只复制快照，不解析协议。
 */
bool Dev_JoyCon_UART_GetInput(
  joycon_uart_t *joycon_h,
  joycon_input_t *input
) {
  assert(joycon_h && input);

  return Dev_JoyCon_Base_GetInput(&joycon_h->base, input);
}

/**
 * @brief 通过 base 操作表复制最近一次 JoyCon 输入。
 * @param joycon_h base 视图；必须指向 joycon_uart_t 的首成员，回调内反转为具体对象。
 * @param input 输出快照；调用方拥有，函数在临界区内覆盖全部字段。
 * @return 已发布快照的 sequence 非零时为 true，否则为 false。
 * @pre 两个指针非空；joycon_h 必须指向完整的具体对象，不能是独立 base 副本。
 * @post input 含最近成功解析的归一化摇杆量；返回值反映是否存在有效帧。
 * @note 私有回调，只在 task 上下文经 ops 表调用；不得在 ISR 中调用。
 */
static bool dev_joycon_uart_getinputop(
  joycon_base_t *joycon_h,
  joycon_input_t *input
) {
  assert(joycon_h && input);

  joycon_uart_t *joycon_uart_h = (joycon_uart_t *)joycon_h;

  Sys_Critical_Enter();
  *input = joycon_uart_h->input;
  bool available = input->sequence != 0u;
  Sys_Critical_Exit();

  return available;
}

/**
 * @brief 复制 JoyCon 设备运行状态快照。
 * @param joycon_h 已由 Init 初始化且未销毁的 JoyCon 对象；调用期间不得并发 Init 或销毁。
 * @param status 输出缓冲区；调用方拥有，函数在临界区内完整覆盖。
 * @pre joycon_h、status 非空；status 的生命周期覆盖调用。
 * @post status 含启动标志、接收/解析/无效帧/错误计数及最后一帧时间和序号，各计数为同一临界区快照。
 * @note 可在其他任务调用，不得在 ISR 中调用；计数只增不减，溢出后自然回绕。
 */
void Dev_JoyCon_UART_GetStatus(
  joycon_uart_t *joycon_h,
  joycon_status_t *status
) {
  assert(joycon_h && status);

  Dev_JoyCon_Base_GetStatus(&joycon_h->base, status);
}

/**
 * @brief 通过 base 操作表复制 JoyCon 运行状态。
 * @param joycon_h base 视图；必须指向 joycon_uart_t 的首成员。
 * @param status 输出缓冲区；调用方拥有，函数在临界区内完整覆盖。
 * @pre 两个指针非空；joycon_h 指向完整具体对象；调用方不得与析构竞态。
 * @post status 含启动标志、计数器和最后一帧元数据的一致快照。
 * @note 私有回调，只在 task 上下文经 ops 表调用；临界区只复制字段，不访问 UART。
 */
static void dev_joycon_uart_getstatusop(
  joycon_base_t *joycon_h,
  joycon_status_t *status
) {
  assert(joycon_h && status);

  joycon_uart_t *joycon_uart_h = (joycon_uart_t *)joycon_h;

  Sys_Critical_Enter();
  *status = (joycon_status_t){
    .is_started = joycon_uart_h->is_started,
    .received_bytes = joycon_uart_h->rx_byte_count,
    .parsed_frames = joycon_uart_h->parsed_frame_count,
    .invalid_frames = joycon_uart_h->invalid_frame_count,
    .rx_overflow_count = joycon_uart_h->rx_overflow_count,
    .uart_error_count = joycon_uart_h->uart_error_count,
    .rearm_error_count = joycon_uart_h->rearm_error_count,
    .last_frame_ms = joycon_uart_h->input.time_ms,
    .sequence = joycon_uart_h->input.sequence,
  };
  Sys_Critical_Exit();
}
