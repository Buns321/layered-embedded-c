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
 * @brief 初始化 UART 端口适配器并注册 Platform 事件回调。
 * @param uart_h 待初始化的端口对象，不得为 NULL；调用方保留所有权。
 * @param name 调试名称，不得为 NULL；指针会被保存，必须在对象生命周期内有效。
 * @param instance 已由板级配置启用的 UART 实例。
 * @retval PLATFORM_STATUS_OK 内嵌基类已初始化且 Platform 回调注册成功。
 * @retval PLATFORM_STATUS_BUSY 该 UART 实例已注册其他 Platform 回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例未启用、回调集合非法或实例编号非法。
 * @pre UART 硬件已由 CubeMX/Core 完成初始化，且调用发生在任务和中断使用该端口之前。
 * @post 无论注册结果如何，内嵌 uart_base 已按 name 初始化、旧回调已清空且 instance 已保存；
 *       注册成功时本端口成为该 Platform UART 回调槽的唯一所有者。
 * @note 不分配动态内存且不主动启动传输；非线程安全，不得与同一端口的初始化或事件并发。
 * @warning 注册失败时不回滚对象字段，调用方不得依赖该端口的 Platform 事件转发。
 */
platform_status_t Dev_UART_Port_Init(
  uart_port_t *uart_h,
  const char *name,
  platform_uart_instance_t instance
);

/** @} */

#endif //LAYERED_EMBEDDED_C_UART_PORT_H
