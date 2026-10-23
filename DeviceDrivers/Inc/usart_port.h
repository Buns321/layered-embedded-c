//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file usart_port.h
 * @brief Platform UART 到 usart_base 的适配器。
 * @addtogroup drivers_concrete
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_USART_PORT_H
#define LAYERED_EMBEDDED_C_USART_PORT_H

#include "platform_usart.h"
#include "usart_base.h"

/**
 * @brief UART 适配器对象。
 */
typedef struct usart_port {
  usart_base_t usart_base;
  Platform_UART_Instance instance;
} usart_port_t;

/**
 * @brief 初始化 UART 适配器并注册 Platform 回调。
 * @param this 对象。
 * @param name 名称。
 * @param instance UART 实例身份。
 * @note Platform 回调会把事件转发到 usart_base 的回调集合。
 * @retval Platform_OK 初始化和回调注册成功。
 * @retval Platform_BUSY 该 UART 实例已注册 Platform 回调。
 * @retval Platform_INVALID_ARGUMENT 参数或实例非法。
 */
Platform_StatusTypeDef Dev_USART_Port_Init(
  usart_port_t *this,
  const char *name,
  Platform_UART_Instance instance
);

/** @} */

#endif //LAYERED_EMBEDDED_C_USART_PORT_H
