//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file uart_port.h
 * @brief Platform UART 到 uart_base 的适配器。
 * @addtogroup drivers_concrete
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_UART_PORT_H
#define LAYERED_EMBEDDED_C_UART_PORT_H

#include "platform_uart.h"
#include "uart_base.h"

/**
 * @brief UART 适配器对象。
 */
typedef struct uart_port_s {
  uart_base_t uart_base;
  platform_uart_instance_t instance;
} uart_port_t;

/**
 * @brief 初始化 UART 适配器并注册 Platform 回调。
 * @param uart_h 对象。
 * @param name 名称。
 * @param instance UART 实例身份。
 * @note Platform 回调会把事件转发到 uart_base 的回调集合。
 * @retval PLATFORM_STATUS_OK 初始化和回调注册成功。
 * @retval PLATFORM_STATUS_BUSY 该 UART 实例已注册 Platform 回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 参数或实例非法。
 */
platform_status_t Dev_UART_Port_Init(
  uart_port_t *uart_h,
  const char *name,
  platform_uart_instance_t instance
);

/** @} */

#endif //LAYERED_EMBEDDED_C_UART_PORT_H
