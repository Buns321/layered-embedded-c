//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file justfloat_uart.c
 * @brief JustFloat UART 遥测协议实现。
 * @ingroup drivers_concrete
 */

#include "justfloat_uart.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static const uint8_t s_justfloat_tail[JUSTFLOAT_FRAME_TAIL_SIZE] = {
    0x00u,
    0x00u,
    0x80u,
    0x7Fu,
};

/**
 * @brief 以小端序写入 IEEE-754 浮点值。
 * @param destination 至少可写 4 字节的目标缓冲区，不得为 NULL；所有权归调用方。
 * @param value 待写入的浮点值，可为 NaN 或无穷；量纲由遥测通道定义。
 * @pre destination 至少覆盖 4 字节，且不与任何正在发送的 DMA 缓冲区并发复用。
 * @post destination[0..3] 依次包含 IEEE-754 位模式的最低到最高字节，不检查或规范化 value。
 * @note 仅执行位复制，不使用浮点解释，也不写入结束符；调用方负责边界和并发控制。
 */
static void dev_justfloat_uart_storefloat(uint8_t *destination, float value) {
  uint32_t bits;
  memcpy(&bits, &value, sizeof(bits));

  destination[0] = (uint8_t)(bits & 0xFFu);
  destination[1] = (uint8_t)((bits >> 8) & 0xFFu);
  destination[2] = (uint8_t)((bits >> 16) & 0xFFu);
  destination[3] = (uint8_t)((bits >> 24) & 0xFFu);
}

/**
 * @brief 把遥测样本编码为 JustFloat 帧并发送。
 * @param telemetry_h 已初始化 JustFloat 对象的 telemetry_base_t 借用视图，不得为 NULL。
 * @param sample 只读样本；前 channel_count 个值按 valid_mask 选择，所有权归调用方。
 * @retval DEV_TELEMETRY_OK 完整帧已交给 DMA，发送完成前 busy 保持 true。
 * @retval DEV_TELEMETRY_BUSY 上一帧仍在发送或 UART 忙；本帧被丢弃并增加 dropped_frames。
 * @retval DEV_TELEMETRY_ERROR 通道数非法或 UART 拒绝；增加 error_count。
 * @pre 对象已初始化并绑定支持 TX DMA 的 UART，样本 valid_mask 位于支持范围。
 * @post 有效位对应值原样写入，无效位写成 NaN；每帧为 4*channel_count 字节小端载荷加 4 字节帧尾。
 *       成功时 tx_buffer 在 busy 清零前保持不变；失败时不启动传输并清除 busy。
 * @note 不阻塞、不分配动态内存；只能由单一任务调用，TX 完成由中断回调更新状态。
 * @warning busy 为 true 时不得改写对象配置或复用 tx_buffer，否则会破坏正在发送的数据。
 */
static dev_telemetry_status_t dev_justfloat_uart_send(telemetry_base_t *telemetry_h,
                                                      const telemetry_sample_t *sample) {
  assert(telemetry_h && sample);

  justfloat_uart_t *justfloat_uart_h = (justfloat_uart_t *)telemetry_h;

  if (!Dev_TelemetryUartTx_TryBegin(&justfloat_uart_h->tx)) {
    return DEV_TELEMETRY_BUSY;
  }

  if (justfloat_uart_h->channel_count == 0u ||
      justfloat_uart_h->channel_count > TELEMETRY_CHANNEL_CAPACITY) {
    Dev_TelemetryUartTx_RecordError(&justfloat_uart_h->tx);
    return DEV_TELEMETRY_ERROR;
  }

  for (uint8_t channel = 0u; channel < justfloat_uart_h->channel_count; channel++) {
    uint32_t mask = (uint32_t)1u << channel;
    float value = (sample->valid_mask & mask) != 0u ? sample->value[channel] : NAN;

    dev_justfloat_uart_storefloat(&justfloat_uart_h->tx_buffer[(uint32_t)channel * 4u], value);
  }

  uint32_t payload_size = (uint32_t)justfloat_uart_h->channel_count * 4u;
  memcpy(&justfloat_uart_h->tx_buffer[payload_size], s_justfloat_tail, JUSTFLOAT_FRAME_TAIL_SIZE);

  uint32_t frame_size = payload_size + JUSTFLOAT_FRAME_TAIL_SIZE;
  return Dev_TelemetryUartTx_SubmitDMA(&justfloat_uart_h->tx, (uint16_t)frame_size);
}

/**
 * @brief 复制遥测发送状态快照。
 * @param telemetry_h 已初始化 JustFloat 对象的 telemetry_base_t 借用视图，不得为 NULL。
 * @param status 状态输出缓冲区，不得为 NULL；所有权归调用方。
 * @pre status 至少可写一个 telemetry_status_t。
 * @post 写入 is_busy、sent_frames、dropped_frames 和 error_count；不修改对象。
 * @note 使用临界区读取发送状态机，不阻塞、不分配动态内存。
 */
static void dev_justfloat_uart_getstatus(telemetry_base_t *telemetry_h,
                                         telemetry_status_t *status) {
  assert(telemetry_h && status);

  justfloat_uart_t *justfloat_uart_h = (justfloat_uart_t *)telemetry_h;
  Dev_TelemetryUartTx_GetStatus(&justfloat_uart_h->tx, status);
}

static const telemetry_ops_t telemetry_ops_justfloat = {
    .send = dev_justfloat_uart_send,
    .get_status = dev_justfloat_uart_getstatus,
};

/**
 * @brief 在 UART 上初始化 JustFloat 遥测对象。
 * @param telemetry_h 待初始化的静态对象，不得为 NULL；调用方保留所有权。
 * @param name 调试名称，不得为 NULL；指针会被保存，必须在对象生命周期内有效。
 * @param uart_h 已初始化且 TX 支持 DMA 的 UART 后端，借用且不得为 NULL。
 * @param channel_count 每帧通道数，范围 1..TELEMETRY_CHANNEL_CAPACITY。
 * @pre telemetry_h 尚未被并发使用，uart_h 当前没有进行中的 TX，也没有其他回调所有者。
 * @post 基类、通道数、忙状态和计数器已初始化，并在 uart_h 上注册 TX 完成回调；
 *       该回调集合会覆盖 uart_h 原有回调，其他回调槽被清空。
 * @note 不启动传输、不阻塞、不分配动态内存；必须在任务和中断使用前由单一装配者调用。
 * @warning 本对象取得 uart_h 的设备级回调槽所有权；不得再注册其他回调或并发初始化，
 *          也不得在已有 DMA 发送时调用，否则旧完成事件将无法归还给原所有者。
 */
void Dev_JustFloat_UART_Init(justfloat_uart_t *telemetry_h, const char *name, uart_base_t *uart_h,
                             uint8_t channel_count) {
  assert(telemetry_h && name && uart_h && channel_count > 0u &&
         channel_count <= TELEMETRY_CHANNEL_CAPACITY);

  Dev_Telemetry_Base_Init(&telemetry_h->base, name, &telemetry_ops_justfloat);

  telemetry_h->channel_count = channel_count;
  Dev_TelemetryUartTx_Init(&telemetry_h->tx, uart_h, telemetry_h->tx_buffer,
                           sizeof(telemetry_h->tx_buffer));
}
