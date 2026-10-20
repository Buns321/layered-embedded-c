//
// Created by Buns_ on 2026/8/26.
//

/**
 * @file usart_base.h
 * @brief USART 抽象：阻塞/IT/DMA 收发与回调分发。
 * @addtogroup drivers_base
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_USART_BASE_H
#define LAYERED_EMBEDDED_C_USART_BASE_H

#include <stdint.h>

typedef struct usart_base usart_base_t;

/**
 * @brief USART 使能状态。
 */
typedef enum {
  DEV_USART_OFF = 0U,
  DEV_USART_ON,
} Dev_USART_State;

/**
 * @brief USART 操作状态。
 */
typedef enum {
  DEV_USART_OK = 0U,
  DEV_USART_BUSY,
  DEV_USART_TIMEOUT,
  DEV_USART_ERROR,
} Dev_USART_Status;

typedef void (*Dev_USART_Callback)(usart_base_t *this, void *context);

/**
 * @brief USART 回调集合。
 * @note 回调在中断上下文执行；注册时内容会被复制。
 */
typedef struct {
  Dev_USART_Callback tx_cplt_cb;
  Dev_USART_Callback tx_halfcplt_cb;
  Dev_USART_Callback rx_cplt_cb;
  Dev_USART_Callback rx_halfcplt_cb;
  Dev_USART_Callback err_cb;
  Dev_USART_Callback abort_cplt_cb;
  Dev_USART_Callback abort_txcplt_cb;
  Dev_USART_Callback abort_rxcplt_cb;
  void *context;
} dev_usart_callbacks_t;

/**
 * @brief USART 具体实现的操作表。
 */
typedef struct usart_ops {
  Dev_USART_Status (*receive)      (usart_base_t *this, uint8_t *buf, uint16_t size, uint32_t timeout);
  Dev_USART_Status (*transmit)     (usart_base_t *this, const uint8_t *data, uint16_t size, uint32_t timeout);
  Dev_USART_Status (*receive_it)   (usart_base_t *this, uint8_t *pdata, uint16_t size);
  Dev_USART_Status (*transmit_it)  (usart_base_t *this, uint8_t *pdata, uint16_t size);
  Dev_USART_Status (*receive_dma)  (usart_base_t *this, uint8_t *pdata, uint16_t size);
  Dev_USART_Status (*transmit_dma) (usart_base_t *this, uint8_t *pdata, uint16_t size);
} usart_ops_t;

/**
 * @brief USART 抽象对象。
 */
typedef struct usart_base {
  const usart_ops_t *ops;
  const char *name;
  Dev_USART_State is_usart_enabled;
  dev_usart_callbacks_t callbacks;
} usart_base_t;

/** @brief 初始化 USART 抽象层并绑定操作表。 */
void             Dev_USART_Base_Init          (usart_base_t *this, const char *name, const usart_ops_t *ops);
/** @brief 读取 USART 状态。 */
Dev_USART_State  Dev_USART_Base_ReadStatus    (const usart_base_t *this);
/** @brief 阻塞接收。 */
Dev_USART_Status Dev_USART_Base_Receive       (usart_base_t *this, uint8_t *buf, uint16_t size, uint32_t timeout);
/** @brief 阻塞发送。 */
Dev_USART_Status Dev_USART_Base_Transmit      (usart_base_t *this, const uint8_t *data, uint16_t size, uint32_t timeout);
/** @brief 中断接收。 */
Dev_USART_Status Dev_USART_Receive_IT         (usart_base_t *this, uint8_t *pdata, uint16_t size);
/** @brief 中断发送。 */
Dev_USART_Status Dev_USART_Transmit_IT        (usart_base_t *this, uint8_t *pdata, uint16_t size);
/** @brief DMA 接收。 */
Dev_USART_Status Dev_USART_Receive_DMA        (usart_base_t *this, uint8_t *pdata, uint16_t size);
/** @brief DMA 发送。 */
Dev_USART_Status Dev_USART_Transmit_DMA       (usart_base_t *this, uint8_t *pdata, uint16_t size);
/**
 * @brief 注册回调集合。
 * @param this USART 对象。
 * @param cb 回调集合，内容会被复制。
 */
void             Dev_USART_RegisterCallbacks  (usart_base_t *this, const dev_usart_callbacks_t *cb);
/** @brief 由具体 port 适配层调用的回调入口。 */
void             Dev_USART_Base_OnTxCplt      (usart_base_t *this);
/** @brief 由具体 port 适配层调用的回调入口。 */
void             Dev_USART_Base_OnTxHalfCplt  (usart_base_t *this);
/** @brief 由具体 port 适配层调用的回调入口。 */
void             Dev_USART_Base_OnRxCplt      (usart_base_t *this);
/** @brief 由具体 port 适配层调用的回调入口。 */
void             Dev_USART_Base_OnRxHalfCplt  (usart_base_t *this);
/** @brief 由具体 port 适配层调用的回调入口。 */
void             Dev_USART_Base_OnError       (usart_base_t *this);
/** @brief 由具体 port 适配层调用的回调入口。 */
void             Dev_USART_Base_OnAbortCplt   (usart_base_t *this);
/** @brief 由具体 port 适配层调用的回调入口。 */
void             Dev_USART_Base_OnAbortTxCplt (usart_base_t *this);
/** @brief 由具体 port 适配层调用的回调入口。 */
void             Dev_USART_Base_OnAbortRxCplt (usart_base_t *this);

/** @} */

#endif //LAYERED_EMBEDDED_C_USART_BASE_H
