//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file firewater_uart.c
 * @brief FireWater UART 遥测协议实现。
 * @ingroup drivers_concrete
 */

#include "firewater_uart.h"
#include "system_math.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

#define FIREWATER_MAX_PRECISION 6u
#define FIREWATER_MAX_SCALED_VALUE 1000000000.0f

static const uint32_t s_pow10[] = {
  1u, 10u, 100u, 1000u, 10000u, 100000u, 1000000u,
};

/**
 * @brief 发送完成后释放忙状态并通知完成。
 * @param uart_h 产生完成事件的 UART 基类视图；本回调不使用其内容，但必须非 NULL。
 * @param context 初始化时注册的 firewater_uart_t 借用指针，不得为 NULL。
 * @pre context 指向已初始化且正等待本次 TX 完成的对象。
 * @post sent_frames 自增 1（按 32 位回绕），busy 置为 false，允许下一次发送。
 * @note 在 TX 完成中断上下文执行；不阻塞、不分配动态内存、不复制发送缓冲区。
 * @warning 只允许一个发送任务与 TX 中断竞争状态；不得在回调中再次启动发送。
 */
static void dev_firewater_uart_ontxcplt(
  uart_base_t *uart_h,
  void *context
) {
  (void)uart_h;
  assert(context);

  firewater_uart_t *firewater_uart_h = (firewater_uart_t *)context;
  firewater_uart_h->sent_frames++;
  firewater_uart_h->busy = false;
}

/**
 * @brief 向容量受限缓冲区追加单字节。
 * @param buffer 目标缓冲区，至少可写 capacity 字节；所有权归调用方。
 * @param capacity 缓冲区总容量，单位字节，必须大于 0。
 * @param length 当前已用长度（字节）的输入/输出指针，不得为 NULL。
 * @param value 待追加字符，按 uint8_t 原样保存。
 * @retval true 已追加 1 字节，length 增加 1。
 * @retval false 缓冲区已满，buffer 与 length 均不变。
 * @pre buffer 至少覆盖 capacity 字节，且 *length 表示有效写入位置。
 * @post 成功时新字节位于 buffer[*length - 1]，且不写入字符串结束符。
 * @note 不做并发保护；调用方必须保证同一缓冲区只有一个写者。
 */
static bool dev_firewater_uart_appendchar(
  uint8_t *buffer,
  uint32_t capacity,
  uint32_t *length,
  char value
) {
  if (*length >= capacity) {
    return false;
  }

  buffer[*length] = (uint8_t)value;
  (*length)++;
  return true;
}

/**
 * @brief 向容量受限缓冲区追加字符串。
 * @param buffer 目标缓冲区，至少可写 capacity 字节；所有权归调用方。
 * @param capacity 缓冲区总容量，单位字节，必须大于 0。
 * @param length 当前已用长度（字节）的输入/输出指针，不得为 NULL。
 * @param value 以 NUL 结尾的只读字符串，不得为 NULL；仅本次调用期间需要有效。
 * @retval true 已追加字符串的全部字节，length 增加 strlen(value)。
 * @retval false 剩余容量不足，buffer 与 length 均不变。
 * @pre *length 不超过 capacity，value 与 buffer 不重叠。
 * @post 成功时不复制结尾 NUL，length 指向下一个可写字节。
 * @note 缓冲区不做并发保护；内存不足时整段字符串原子拒绝。
 */
static bool dev_firewater_uart_appendstring(
  uint8_t *buffer,
  uint32_t capacity,
  uint32_t *length,
  const char *value
) {
  uint32_t value_length = (uint32_t)strlen(value);

  if (
    *length > capacity ||
    value_length > capacity - *length
  ) {
    return false;
  }

  memcpy(&buffer[*length], value, value_length);
  *length += value_length;
  return true;
}

/**
 * @brief 以十进制向容量受限缓冲区追加无符号数。
 * @param buffer 目标缓冲区，至少可写 capacity 字节；所有权归调用方。
 * @param capacity 缓冲区总容量，单位字节，必须大于 0。
 * @param length 当前已用长度（字节）的输入/输出指针，不得为 NULL。
 * @param value 待格式化的无符号值，范围 0..UINT32_MAX。
 * @retval true 已写入最短十进制表示，length 增加实际位数。
 * @retval false 剩余容量不足，buffer 与 length 均不变。
 * @pre *length 不超过 capacity。
 * @post 成功时不写入前导零或字符串结束符。
 * @note 只使用固定 10 字节栈空间，不分配动态内存；调用方负责串行化缓冲区访问。
 */
static bool dev_firewater_uart_appendunsigned(
  uint8_t *buffer,
  uint32_t capacity,
  uint32_t *length,
  uint32_t value
) {
  char digits[10];
  uint32_t digit_count = 0u;

  do {
    digits[digit_count++] = (char)('0' + (value % 10u));
    value /= 10u;
  } while (value != 0u);

  if (
    *length > capacity ||
    digit_count > capacity - *length
  ) {
    return false;
  }

  while (digit_count > 0u) {
    buffer[(*length)++] = (uint8_t)digits[--digit_count];
  }

  return true;
}

/**
 * @brief 以指定精度追加有限浮点值。
 * @param buffer 目标缓冲区，至少可写 capacity 字节；所有权归调用方。
 * @param capacity 缓冲区总容量，单位字节，必须大于 0。
 * @param length 当前已用长度（字节）的输入/输出指针，不得为 NULL。
 * @param value 待格式化的有限浮点量；量纲由遥测通道定义。
 * @param precision 小数位数，范围 0..6。
 * @retval true 已按四舍五入（半值远离零）写入指定精度表示。
 * @retval false value 非有限、precision 越界、绝对值超出可表示范围或容量不足。
 * @pre |value| * 10^precision 必须不超过 1000000000，才能保证安全转换。
 * @post 成功时最多写入符号、整数、小数点和 precision 位小数，不含 NUL；
 *       失败时 length 可能已因缓冲区耗尽而前进并留下部分前缀，调用方不得发送该帧。
 * @note 不分配动态内存；调用方负责串行化缓冲区访问并丢弃任何失败编码。
 */
static bool dev_firewater_uart_appendvalue(
  uint8_t *buffer,
  uint32_t capacity,
  uint32_t *length,
  float value,
  uint8_t precision
) {
  if (!SYS_ISFINITE(value) || precision > FIREWATER_MAX_PRECISION) {
    return false;
  }

  uint32_t scale = s_pow10[precision];
  float scaled_value = value * (float)scale;

  if (
    !SYS_ISFINITE(scaled_value) ||
    SYS_ABS(scaled_value) > FIREWATER_MAX_SCALED_VALUE
  ) {
    return false;
  }

  int32_t integer_scaled = scaled_value >= 0.0f
    ? (int32_t)(scaled_value + 0.5f)
    : (int32_t)(scaled_value - 0.5f);

  uint32_t magnitude = integer_scaled < 0
    ? (uint32_t)(-integer_scaled)
    : (uint32_t)integer_scaled;

  if (integer_scaled < 0) {
    if (!dev_firewater_uart_appendchar(buffer, capacity, length, '-')) {
      return false;
    }
  }

  uint32_t integer_part = magnitude / scale;
  uint32_t decimal_part = magnitude % scale;

  if (!dev_firewater_uart_appendunsigned(
        buffer,
        capacity,
        length,
        integer_part)) {
    return false;
  }

  if (precision == 0u) {
    return true;
  }

  if (!dev_firewater_uart_appendchar(buffer, capacity, length, '.')) {
    return false;
  }

  for (uint8_t divisor_index = precision; divisor_index > 0u; divisor_index--) {
    uint32_t divisor = s_pow10[divisor_index - 1u];
    char digit = (char)('0' + ((decimal_part / divisor) % 10u));

    if (!dev_firewater_uart_appendchar(buffer, capacity, length, digit)) {
      return false;
    }
  }

  return true;
}

/**
 * @brief 将遥测样本编码为 FireWater 帧。
 * @param firewater_uart_h 已初始化对象，借用且不得为 NULL。
 * @param sample 只读样本；只使用前 channel_count 个通道及 valid_mask，所有权归调用方。
 * @param frame_size 输出帧长度（字节）的指针，不得为 NULL。
 * @retval true 已生成 name:value[,...]\n 帧，并写入 frame_size。
 * @retval false 没有有效通道、值非有限或超范围、或 512 字节容量不足。
 * @pre 对象处于空闲状态，channel_names 与 channel_count 配置有效，且没有 DMA 正在读取 tx_buffer。
 * @post 成功时 tx_buffer 前 frame_size 字节有效、以换行结束且不含 NUL；
 *       失败时 frame_size 不变，tx_buffer 内容未定义。缓冲区所有权始终归对象。
 * @note 仅执行 CPU 编码，不启动 UART、不修改 busy；非线程安全，仅由单一发送任务调用。
 */
static bool dev_firewater_uart_encode(
  firewater_uart_t *firewater_uart_h,
  const telemetry_sample_t *sample,
  uint32_t *frame_size
) {
  uint32_t length = 0u;
  bool has_value = false;

  for (uint8_t channel = 0u; channel < firewater_uart_h->channel_count; channel++) {
    uint32_t mask = (uint32_t)1u << channel;
    if ((sample->valid_mask & mask) == 0u) {
      continue;
    }

    if (has_value) {
      if (!dev_firewater_uart_appendchar(
            firewater_uart_h->tx_buffer,
            FIREWATER_UART_TX_CAPACITY,
            &length,
            ',')) {
        return false;
      }
    }

    if (
      !dev_firewater_uart_appendstring(
        firewater_uart_h->tx_buffer,
        FIREWATER_UART_TX_CAPACITY,
        &length,
        firewater_uart_h->channel_names[channel]) ||
      !dev_firewater_uart_appendchar(
        firewater_uart_h->tx_buffer,
        FIREWATER_UART_TX_CAPACITY,
        &length,
        ':') ||
      !dev_firewater_uart_appendvalue(
        firewater_uart_h->tx_buffer,
        FIREWATER_UART_TX_CAPACITY,
        &length,
        sample->value[channel],
        firewater_uart_h->precision)
    ) {
      return false;
    }

    has_value = true;
  }

  if (!has_value) {
    return false;
  }

  if (!dev_firewater_uart_appendchar(
        firewater_uart_h->tx_buffer,
        FIREWATER_UART_TX_CAPACITY,
        &length,
        '\n')) {
    return false;
  }

  *frame_size = length;
  return true;
}

/**
 * @brief 编码并启动遥测帧异步发送。
 * @param telemetry_h 已初始化 FireWater 对象的 telemetry_base_t 借用视图，不得为 NULL。
 * @param sample 只读样本；编码期间访问，返回后不保留，所有权归调用方。
 * @retval DEV_TELEMETRY_OK 整帧已交给 DMA，发送完成前 busy 保持 true。
 * @retval DEV_TELEMETRY_BUSY 上一帧仍在发送或 UART 忙；本帧被丢弃并增加 dropped_frames。
 * @retval DEV_TELEMETRY_ERROR 无可发送通道、值非法/超范围、编码溢出或 UART 拒绝；增加 error_count。
 * @pre telemetry_h 已绑定有效且支持 TX DMA 的 UART；样本 valid_mask 位于支持范围且有效值有限。
 * @post 成功时 tx_buffer 在 busy 清零前保持不变；失败时不启动传输并清除 busy。
 * @note 不阻塞、不分配动态内存；只能由单一任务调用，TX 完成由中断回调更新状态。
 * @warning busy 为 true 时不得改写对象配置或复用 tx_buffer，否则会破坏正在发送的数据。
 */
static dev_telemetry_status_t dev_firewater_uart_send(
  telemetry_base_t *telemetry_h,
  const telemetry_sample_t *sample
) {
  assert(telemetry_h && sample);

  firewater_uart_t *firewater_uart_h = (firewater_uart_t *)telemetry_h;

  if (firewater_uart_h->busy) {
    firewater_uart_h->dropped_frames++;
    return DEV_TELEMETRY_BUSY;
  }

  uint32_t frame_size = 0u;
  if (!dev_firewater_uart_encode(firewater_uart_h, sample, &frame_size)) {
    firewater_uart_h->error_count++;
    return DEV_TELEMETRY_ERROR;
  }

  firewater_uart_h->busy = true;
  dev_uart_status_t status = Dev_UART_TransmitDMA(
    firewater_uart_h->uart_h,
    firewater_uart_h->tx_buffer,
    (uint16_t)frame_size
  );

  if (status != DEV_UART_OK) {
    firewater_uart_h->busy = false;

    if (status == DEV_UART_BUSY) {
      firewater_uart_h->dropped_frames++;
      return DEV_TELEMETRY_BUSY;
    }

    firewater_uart_h->error_count++;
    return DEV_TELEMETRY_ERROR;
  }

  return DEV_TELEMETRY_OK;
}

/**
 * @brief 复制遥测发送状态快照。
 * @param telemetry_h 已初始化 FireWater 对象的 telemetry_base_t 借用视图，不得为 NULL。
 * @param status 状态输出缓冲区，不得为 NULL；所有权归调用方。
 * @pre status 至少可写一个 telemetry_status_t。
 * @post 写入 is_busy、sent_frames、dropped_frames 和 error_count；不修改对象。
 * @note 各字段读取期间可能与 TX 完成中断交错，快照不保证字段严格同属一个时刻；
 *       计数器按 uint32_t 回绕，不阻塞、不分配动态内存。
 */
static void dev_firewater_uart_getstatus(
  telemetry_base_t *telemetry_h,
  telemetry_status_t *status
) {
  assert(telemetry_h && status);

  firewater_uart_t *firewater_uart_h = (firewater_uart_t *)telemetry_h;
  *status = (telemetry_status_t){
    .is_busy = firewater_uart_h->busy,
    .sent_frames = firewater_uart_h->sent_frames,
    .dropped_frames = firewater_uart_h->dropped_frames,
    .error_count = firewater_uart_h->error_count,
  };
}

static const telemetry_ops_t telemetry_ops_firewater = {
  .send       = dev_firewater_uart_send,
  .get_status = dev_firewater_uart_getstatus,
};

/**
 * @brief 在 UART 上初始化 FireWater 遥测对象。
 * @param telemetry_h 待初始化的静态对象，不得为 NULL；调用方保留所有权。
 * @param name 调试名称，不得为 NULL；指针会被保存，必须在对象生命周期内有效。
 * @param uart_h 已初始化且 TX 支持 DMA 的 UART 后端，借用且不得为 NULL。
 * @param channel_names 至少 channel_count 项的通道名称表，不得为 NULL；
 *        每个名称非空、不含逗号和冒号，且指针和字符串在对象生命周期内有效。
 * @param channel_count 通道数，范围 1..TELEMETRY_CHANNEL_CAPACITY。
 * @param precision 小数位数，范围 0..6。
 * @pre telemetry_h 尚未被并发使用，uart_h 当前没有进行中的 TX，也没有其他回调所有者。
 * @post 基类、配置、忙状态和计数器已初始化，并在 uart_h 上注册 TX 完成回调；
 *       该回调集合会覆盖 uart_h 原有回调，其他回调槽被清空。
 * @note 不启动传输、不阻塞、不分配动态内存；必须在任务和中断使用前由单一装配者调用。
 * @warning 本对象取得 uart_h 的设备级回调槽所有权；不得再注册其他回调或并发初始化，
 *          也不得在已有 DMA 发送时调用，否则旧完成事件将无法归还给原所有者。
 */
void Dev_FireWater_UART_Init(
  firewater_uart_t *telemetry_h,
  const char *name,
  uart_base_t *uart_h,
  const char *const channel_names[],
  uint8_t channel_count,
  uint8_t precision
) {
  assert(
    telemetry_h &&
    name &&
    uart_h &&
    channel_names &&
    channel_count > 0u &&
    channel_count <= TELEMETRY_CHANNEL_CAPACITY &&
    precision <= FIREWATER_MAX_PRECISION
  );

  for (uint8_t channel = 0u; channel < channel_count; channel++) {
    assert(
      channel_names[channel] &&
      channel_names[channel][0] != '\0' &&
      strchr(channel_names[channel], ',') == NULL &&
      strchr(channel_names[channel], ':') == NULL
    );
  }

  Dev_Telemetry_Base_Init(
    &telemetry_h->base,
    name,
    &telemetry_ops_firewater
  );

  telemetry_h->uart_h = uart_h;
  telemetry_h->channel_names = channel_names;
  telemetry_h->channel_count = channel_count;
  telemetry_h->precision = precision;
  telemetry_h->busy = false;
  telemetry_h->sent_frames = 0u;
  telemetry_h->dropped_frames = 0u;
  telemetry_h->error_count = 0u;

  dev_uart_callbacks_t callbacks = {
    .tx_complete_cb = dev_firewater_uart_ontxcplt,
    .context = telemetry_h,
  };
  Dev_UART_RegisterCallback(uart_h, &callbacks);
}
