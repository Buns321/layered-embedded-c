/**
 * @file telemetry_uart_tx.c
 * @brief UART DMA 遥测发送状态机与计数公共前端实现。
 * @ingroup drivers
 */

#include "telemetry_uart_tx.h"
#include "system_critical.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

/**
 * @brief UART TX 完成中断回调：累加发送帧数并释放忙标志。
 * @param uart_h 触发事件的 UART 后端，不得为 NULL。
 * @param context 初始化时绑定的 telemetry_uart_tx_t 指针，不得为 NULL。
 * @pre 仅在 TX 完成中断上下文执行。
 * @post sent_frames 自增 1，busy 置为 false。
 * @note 不阻塞，不重新启动发送。
 */
static void dev_telemetryuarttx_ontxcplt(uart_base_t *uart_h, void *context) {
  (void)uart_h;
  assert(context != NULL);

  telemetry_uart_tx_t *tx_h = (telemetry_uart_tx_t *)context;
  tx_h->sent_frames++;
  tx_h->busy = false;
}

/**
 * @brief 初始化 UART 遥测发送状态机并接管 UART TX 完成回调槽。
 * @param tx_h 待初始化的发送状态机，不得为 NULL。
 * @param uart_h 已初始化且支持 TX DMA 的 UART 后端，不得为 NULL；调用方保留所有权。
 * @param tx_buffer 调用方静态持有的发送缓冲区，不得为 NULL。
 * @param tx_capacity_bytes 发送缓冲区字节容量，单位 byte；必须大于 0。
 * @pre 只在单线程初始化阶段调用，uart_h 当前无进行中传输且无其他回调所有者。
 * @post tx_h 已清零并绑定缓冲区，uart_h 回调槽已注册内部 TX 完成回调。
 * @note 不启动传输、不阻塞、不分配动态内存。
 * @warning 注册回调会覆盖 uart_h 原有回调槽。
 */
void Dev_TelemetryUartTx_Init(telemetry_uart_tx_t *tx_h, uart_base_t *uart_h, uint8_t *tx_buffer,
                              uint16_t tx_capacity_bytes) {
  assert(tx_h != NULL && uart_h != NULL && tx_buffer != NULL && tx_capacity_bytes > 0u);

  (void)memset(tx_h, 0, sizeof(*tx_h));
  tx_h->uart_h = uart_h;
  tx_h->tx_buffer = tx_buffer;
  tx_h->tx_capacity_bytes = tx_capacity_bytes;

  const dev_uart_callbacks_t callbacks = {
      .tx_complete_cb = dev_telemetryuarttx_ontxcplt,
      .context = tx_h,
  };
  Dev_UART_RegisterCallback(uart_h, &callbacks);
}

/**
 * @brief 检查发送通道是否空闲；若忙则计入丢帧。
 * @param tx_h 已初始化的发送状态机，不得为 NULL。
 * @retval true 发送通道空闲，调用方可安全填充 tx_buffer 并调用 SubmitDMA。
 * @retval false 上一帧仍在发送，dropped_frames 已自增 1。
 * @pre 仅允许单个发送任务调用。
 * @post 返回 false 时 dropped_frames 递增；不修改 tx_buffer。
 * @note 不阻塞、不等待 DMA。
 */
bool Dev_TelemetryUartTx_TryBegin(telemetry_uart_tx_t *tx_h) {
  assert(tx_h != NULL);

  if (tx_h->busy) {
    tx_h->dropped_frames++;
    return false;
  }
  return true;
}

/**
 * @brief 记录一次协议配置或编码错误。
 * @param tx_h 已初始化的发送状态机，不得为 NULL。
 * @post error_count 自增 1。
 * @note 仅在编码/校验失败且未启动 DMA 时调用。
 */
void Dev_TelemetryUartTx_RecordError(telemetry_uart_tx_t *tx_h) {
  assert(tx_h != NULL);
  tx_h->error_count++;
}

/**
 * @brief 提交已编码的帧到 UART DMA 并统一映射忙/错误状态。
 * @param tx_h 已初始化的发送状态机，不得为 NULL。
 * @param frame_size_bytes 已写入 tx_buffer 的帧长度，单位 byte；不得超过容量。
 * @retval DEV_TELEMETRY_OK 帧已提交 DMA，完成前 busy 保持 true。
 * @retval DEV_TELEMETRY_BUSY UART 后端忙，busy 已恢复 false 且 dropped_frames 递增。
 * @retval DEV_TELEMETRY_ERROR UART 后端错误，busy 已恢复 false 且 error_count 递增。
 * @pre tx_buffer[0..frame_size_bytes-1] 已编码完成，且 TryBegin 返回 true。
 * @post 成功时 busy 为 true；失败时 busy 为 false 并更新对应计数器。
 * @note 不阻塞等待传输完成。
 */
dev_telemetry_status_t Dev_TelemetryUartTx_SubmitDMA(telemetry_uart_tx_t *tx_h,
                                                     uint16_t frame_size_bytes) {
  assert(tx_h != NULL && tx_h->uart_h != NULL && tx_h->tx_buffer != NULL);
  assert(frame_size_bytes > 0u && frame_size_bytes <= tx_h->tx_capacity_bytes);

  tx_h->busy = true;
  dev_uart_status_t status = Dev_UART_TransmitDMA(tx_h->uart_h, tx_h->tx_buffer, frame_size_bytes);

  if (status != DEV_UART_OK) {
    tx_h->busy = false;
    if (status == DEV_UART_BUSY) {
      tx_h->dropped_frames++;
      return DEV_TELEMETRY_BUSY;
    }
    tx_h->error_count++;
    return DEV_TELEMETRY_ERROR;
  }

  return DEV_TELEMETRY_OK;
}

/**
 * @brief 在临界区内读取遥测发送状态快照。
 * @param tx_h 已初始化的发送状态机，不得为 NULL。
 * @param status 输出状态缓冲区，不得为 NULL。
 * @pre 仅在任务上下文调用。
 * @post status 写入一致的 is_busy、sent_frames、dropped_frames 与 error_count。
 * @note 不修改状态机或计数器。
 */
void Dev_TelemetryUartTx_GetStatus(const telemetry_uart_tx_t *tx_h, telemetry_status_t *status) {
  assert(tx_h != NULL && status != NULL);

  Sys_Critical_Enter();
  *status = (telemetry_status_t){
      .is_busy = tx_h->busy,
      .sent_frames = tx_h->sent_frames,
      .dropped_frames = tx_h->dropped_frames,
      .error_count = tx_h->error_count,
  };
  Sys_Critical_Exit();
}
