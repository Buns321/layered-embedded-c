//
// 由 Buns_ 创建于 2026/9/2。
//

/**
 * @file rtos_uart.h
 * @brief 串口任务接口。
 * @addtogroup rtos
 * @{
 * @note 板级强符号实现覆盖 Core/Src/freertos.c 中的弱占位。
 */

#ifndef LAYERED_EMBEDDED_C_RTOS_UART_H
#define LAYERED_EMBEDDED_C_RTOS_UART_H

/**
 * @brief WHT101 与 JoyCon 的 UART 处理任务。
 * @param argument CMSIS-RTOS2 任务参数，当前未使用。
 * @note 中断只负责入队；协议解析和上电归零状态机在本任务上下文运行。
 */
void RTOS_UART_StartTask(void *argument);

/** @} */

#endif //LAYERED_EMBEDDED_C_RTOS_UART_H
