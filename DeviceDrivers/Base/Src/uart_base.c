//
// 由 Buns_ 创建于 2026/8/26。
//

/**
 * @file uart_base.c
 * @brief UART 抽象接口实现。
 * @ingroup drivers_base
 */

#include "uart_base.h"
#include <assert.h>

/** @brief 初始化 UART 抽象层并绑定操作表。 */
void Dev_UART_Base_Init(uart_base_t *uart_h, const char *name, const uart_ops_t *ops) {
  assert(uart_h && name && ops);
  uart_h->ops              = ops;
  uart_h->name             = name;
  uart_h->is_uart_enabled = DEV_UART_ON;
  uart_h->callbacks        = (dev_uart_callbacks_t){0};
}

/** @brief 读取 UART 状态。 */
dev_uart_state_t Dev_UART_Base_ReadStatus(const uart_base_t *uart_h) {
  assert((uart_h));
  return uart_h->is_uart_enabled;
}

/** @brief 阻塞接收。 */
dev_uart_status_t Dev_UART_Base_Receive(uart_base_t *uart_h, uint8_t *buf, uint16_t size, uint32_t timeout) {
  assert(uart_h && uart_h->ops && uart_h->ops->receive && buf && size != 0u);
  return uart_h->ops->receive(uart_h, buf, size, timeout);
}

/** @brief 阻塞发送。 */
dev_uart_status_t Dev_UART_Base_Transmit(uart_base_t *uart_h, const uint8_t *data, uint16_t size, uint32_t timeout) {
  assert(uart_h && uart_h->ops && uart_h->ops->transmit && data && size != 0u);
  return uart_h->ops->transmit(uart_h, data, size, timeout);
}

/** @brief 中断接收。 */
dev_uart_status_t Dev_UART_ReceiveIT(uart_base_t *uart_h, uint8_t *pdata, uint16_t size) {
  assert(uart_h && uart_h->ops && uart_h->ops->receive_it && pdata && size != 0u);
  return uart_h->ops->receive_it(uart_h, pdata, size);
}

/** @brief 中断发送。 */
dev_uart_status_t Dev_UART_TransmitIT(uart_base_t *uart_h, const uint8_t *pdata, uint16_t size){
  assert(uart_h && uart_h->ops && uart_h->ops->transmit_it && pdata && size != 0u);
  return uart_h->ops->transmit_it(uart_h, pdata, size);
}

/** @brief DMA 接收。 */
dev_uart_status_t Dev_UART_ReceiveDMA(uart_base_t *uart_h, uint8_t *pdata, uint16_t size) {
  assert(uart_h && uart_h->ops && uart_h->ops->receive_dma && pdata && size != 0u);
  return uart_h->ops->receive_dma(uart_h, pdata, size);
}

/** @brief DMA 发送。 */
dev_uart_status_t Dev_UART_TransmitDMA(uart_base_t *uart_h, const uint8_t *pdata, uint16_t size) {
  assert(uart_h && uart_h->ops && uart_h->ops->transmit_dma && pdata && size != 0u);
  return uart_h->ops->transmit_dma(uart_h, pdata, size);
}

/**
 * @brief 注册回调集合。
 * @param uart_h UART 对象。
 * @param cb 回调集合，内容会被复制。
 */
void Dev_UART_RegisterCallback(uart_base_t *uart_h, const dev_uart_callbacks_t *cb) {
  assert(uart_h && cb);
  uart_h->callbacks = *cb;
}

/** @brief 由具体 port 适配层调用的回调入口。 */
void Dev_UART_Base_OnTxComplete(uart_base_t *uart_h) {
  assert(uart_h);
  if (uart_h->callbacks.tx_complete_cb) {
    uart_h->callbacks.tx_complete_cb(uart_h, uart_h->callbacks.context);
  }
}

/** @brief 由具体 port 适配层调用的回调入口。 */
void Dev_UART_Base_OnTxHalfComplete(uart_base_t *uart_h){
  assert(uart_h);
  if (uart_h->callbacks.tx_half_complete_cb) {
    uart_h->callbacks.tx_half_complete_cb(uart_h, uart_h->callbacks.context);
  }
}

/** @brief 由具体 port 适配层调用的回调入口。 */
void Dev_UART_Base_OnRxComplete(uart_base_t *uart_h) {
  assert(uart_h);
  if (uart_h->callbacks.rx_complete_cb) {
    uart_h->callbacks.rx_complete_cb(uart_h, uart_h->callbacks.context);
  }
}

/** @brief 由具体 port 适配层调用的回调入口。 */
void Dev_UART_Base_OnRxHalfComplete(uart_base_t *uart_h){
  assert(uart_h);
  if (uart_h->callbacks.rx_half_complete_cb) {
    uart_h->callbacks.rx_half_complete_cb(uart_h, uart_h->callbacks.context);
  }
}

/** @brief 由具体 port 适配层调用的回调入口。 */
void Dev_UART_Base_OnError(uart_base_t *uart_h){
  assert(uart_h);
  if (uart_h->callbacks.err_cb) {
    uart_h->callbacks.err_cb(uart_h, uart_h->callbacks.context);
  }
}

/** @brief 由具体 port 适配层调用的回调入口。 */
void Dev_UART_Base_OnAbortComplete(uart_base_t *uart_h){
  assert(uart_h);
  if (uart_h->callbacks.abort_complete_cb) {
    uart_h->callbacks.abort_complete_cb(uart_h, uart_h->callbacks.context);
  }
}

/** @brief 由具体 port 适配层调用的回调入口。 */
void Dev_UART_Base_OnAbortTxComplete(uart_base_t *uart_h){
  assert(uart_h);
  if (uart_h->callbacks.abort_tx_complete_cb) {
    uart_h->callbacks.abort_tx_complete_cb(uart_h, uart_h->callbacks.context);
  }
}

/** @brief 由具体 port 适配层调用的回调入口。 */
void Dev_UART_Base_OnAbortRxComplete(uart_base_t *uart_h){
  assert(uart_h);
  if (uart_h->callbacks.abort_rx_complete_cb) {
    uart_h->callbacks.abort_rx_complete_cb(uart_h, uart_h->callbacks.context);
  }
}
