//
// Created by Buns_ on 2026/8/24.
//

/**
 * @file platform_usart.h
 * @brief USART 抽象：收发、IT/DMA、回调注册与错误清理。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_USART_H
#define LAYERED_EMBEDDED_C_PLATFORM_USART_H

#include "platform_def.h"
#include <stdint.h>

#define TOTAL_USART_NUM 6

/**
 * @brief UART 实例身份。
 */
typedef enum {
  PLATFORM_USART_1 = 0U,
  PLATFORM_USART_2,
  PLATFORM_USART_3,
  PLATFORM_UART_4,
  PLATFORM_UART_5,
  PLATFORM_USART_6,
} Platform_UART_Instance;

typedef void (*Platform_UART_Callback)(Platform_UART_Instance uart, void *context);

/**
 * @brief Platform UART 回调集合。
 * @note 回调在中断上下文执行，回调内不要做阻塞操作。
 * @note 注册时结构体内容会被复制，调用方无需保持其生命周期。
 */
typedef struct {
  Platform_UART_Callback tx_cplt_cb;
  Platform_UART_Callback tx_halfcplt_cb;
  Platform_UART_Callback rx_cplt_cb;
  Platform_UART_Callback rx_halfcplt_cb;
  Platform_UART_Callback err_cb;
  Platform_UART_Callback abort_cplt_cb;
  Platform_UART_Callback abort_txcplt_cb;
  Platform_UART_Callback abort_rxcplt_cb;
  void *context;
} platform_uart_callbacks_t;

/** @brief 阻塞接收。 */
Platform_StatusTypeDef Platform_UART_Receive           (Platform_UART_Instance uart, uint8_t *pdata, uint16_t size, uint32_t timeout);
/** @brief 阻塞发送。 */
Platform_StatusTypeDef Platform_UART_Transmit          (Platform_UART_Instance uart, uint8_t *pdata, uint16_t size, uint32_t timeout);
/** @brief 中断接收。 */
Platform_StatusTypeDef Platform_UART_Receive_IT        (Platform_UART_Instance uart, uint8_t *pdata, uint16_t size);
/** @brief 中断发送。 */
Platform_StatusTypeDef Platform_UART_Transmit_IT       (Platform_UART_Instance uart, uint8_t *pdata, uint16_t size);
/** @brief DMA 接收。 */
Platform_StatusTypeDef Platform_UART_Receive_DMA       (Platform_UART_Instance uart, uint8_t *pdata, uint16_t size);
/** @brief DMA 发送。 */
Platform_StatusTypeDef Platform_UART_Transmit_DMA      (Platform_UART_Instance uart, uint8_t *pdata, uint16_t size);
/**
 * @brief 中止接收并清除 UART 错误/状态标志。
 * @param uart UART 实例。
 * @retval Platform_OK 成功或本来就不忙。
 * @retval Platform_ERROR 失败。
 */
Platform_StatusTypeDef Platform_UART_ClearBuffer       (Platform_UART_Instance uart);
/**
 * @brief 注册 UART 回调。
 * @param uart UART 实例。
 * @param cb 回调集合，内容会被复制。
 */
void                   Platform_UART_RegisterCallbacks (Platform_UART_Instance uart, const platform_uart_callbacks_t *cb);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_USART_H
