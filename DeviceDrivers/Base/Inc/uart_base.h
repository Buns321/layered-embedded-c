//
// 由 Buns_ 创建于 2026/8/26。
//

/**
 * @file uart_base.h
 * @brief UART 抽象：阻塞/IT/DMA 收发与回调分发。
 * @addtogroup drivers_base
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_UART_BASE_H
#define LAYERED_EMBEDDED_C_UART_BASE_H

#include <stdint.h>

typedef struct uart_base_s uart_base_t;

/**
 * @brief UART 使能状态。
 */
typedef enum dev_uart_state_e {
  DEV_UART_OFF = 0U,
  DEV_UART_ON,
} dev_uart_state_t;

/**
 * @brief UART 操作状态。
 */
typedef enum dev_uart_status_e {
  DEV_UART_OK = 0U,
  DEV_UART_BUSY,
  DEV_UART_TIMEOUT,
  DEV_UART_ERROR,
} dev_uart_status_t;

typedef void (*dev_uart_callback_t)(uart_base_t *uart_h, void *context);

/**
 * @brief UART 回调集合。
 * @note 回调在中断上下文执行；注册时内容会被复制。
 */
typedef struct dev_uart_callbacks_s {
  dev_uart_callback_t tx_complete_cb;
  dev_uart_callback_t tx_half_complete_cb;
  dev_uart_callback_t rx_complete_cb;
  dev_uart_callback_t rx_half_complete_cb;
  dev_uart_callback_t err_cb;
  dev_uart_callback_t abort_complete_cb;
  dev_uart_callback_t abort_tx_complete_cb;
  dev_uart_callback_t abort_rx_complete_cb;
  void *context;
} dev_uart_callbacks_t;

/**
 * @brief UART 具体实现的操作表。
 */
typedef struct uart_ops_s {
  dev_uart_status_t (*receive)      (uart_base_t *uart_h, uint8_t *buf, uint16_t size, uint32_t timeout);
  dev_uart_status_t (*transmit)     (uart_base_t *uart_h, const uint8_t *data, uint16_t size, uint32_t timeout);
  dev_uart_status_t (*receive_it)   (uart_base_t *uart_h, uint8_t *pdata, uint16_t size);
  dev_uart_status_t (*transmit_it)  (uart_base_t *uart_h, const uint8_t *pdata, uint16_t size);
  dev_uart_status_t (*receive_dma)  (uart_base_t *uart_h, uint8_t *pdata, uint16_t size);
  dev_uart_status_t (*transmit_dma) (uart_base_t *uart_h, const uint8_t *pdata, uint16_t size);
} uart_ops_t;

/**
 * @brief UART 抽象对象。
 */
typedef struct uart_base_s {
  const uart_ops_t *ops;
  const char *name;
  dev_uart_state_t is_uart_enabled;
  dev_uart_callbacks_t callbacks;
} uart_base_t;

/** @brief 初始化 UART 抽象层并绑定操作表。 */
void             Dev_UART_Base_Init          (uart_base_t *uart_h, const char *name, const uart_ops_t *ops);
/** @brief 读取 UART 状态。 */
dev_uart_state_t  Dev_UART_Base_ReadStatus    (const uart_base_t *uart_h);
/** @brief 阻塞接收。 */
dev_uart_status_t Dev_UART_Base_Receive       (uart_base_t *uart_h, uint8_t *buf, uint16_t size, uint32_t timeout);
/** @brief 阻塞发送。 */
dev_uart_status_t Dev_UART_Base_Transmit      (uart_base_t *uart_h, const uint8_t *data, uint16_t size, uint32_t timeout);
/** @brief 中断接收。 */
dev_uart_status_t Dev_UART_ReceiveIT         (uart_base_t *uart_h, uint8_t *pdata, uint16_t size);
/** @brief 中断发送。 */
dev_uart_status_t Dev_UART_TransmitIT        (uart_base_t *uart_h, const uint8_t *pdata, uint16_t size);
/** @brief DMA 接收。 */
dev_uart_status_t Dev_UART_ReceiveDMA        (uart_base_t *uart_h, uint8_t *pdata, uint16_t size);
/** @brief DMA 发送。 */
dev_uart_status_t Dev_UART_TransmitDMA       (uart_base_t *uart_h, const uint8_t *pdata, uint16_t size);
/**
 * @brief 注册回调集合。
 * @param uart_h UART 对象。
 * @param cb 回调集合，内容会被复制。
 */
void             Dev_UART_RegisterCallback  (uart_base_t *uart_h, const dev_uart_callbacks_t *cb);
/** @brief 由具体 port 适配层调用的回调入口。 */
void             Dev_UART_Base_OnTxComplete      (uart_base_t *uart_h);
/** @brief 由具体 port 适配层调用的回调入口。 */
void             Dev_UART_Base_OnTxHalfComplete  (uart_base_t *uart_h);
/** @brief 由具体 port 适配层调用的回调入口。 */
void             Dev_UART_Base_OnRxComplete      (uart_base_t *uart_h);
/** @brief 由具体 port 适配层调用的回调入口。 */
void             Dev_UART_Base_OnRxHalfComplete  (uart_base_t *uart_h);
/** @brief 由具体 port 适配层调用的回调入口。 */
void             Dev_UART_Base_OnError       (uart_base_t *uart_h);
/** @brief 由具体 port 适配层调用的回调入口。 */
void             Dev_UART_Base_OnAbortComplete   (uart_base_t *uart_h);
/** @brief 由具体 port 适配层调用的回调入口。 */
void             Dev_UART_Base_OnAbortTxComplete (uart_base_t *uart_h);
/** @brief 由具体 port 适配层调用的回调入口。 */
void             Dev_UART_Base_OnAbortRxComplete (uart_base_t *uart_h);

/** @} */

#endif //LAYERED_EMBEDDED_C_UART_BASE_H
