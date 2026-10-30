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
 * @brief WHT101 与 JoyCon 的 500 Hz UART 处理任务入口。
 * @param argument CMSIS-RTOS2 任务参数；当前忽略，允许为 NULL。
 * @pre 仅在调度器启动后由内核调用，且 App_Car_Init 已完成 UART、IMU 和 JoyCon 初始化。
 * @post 每 2 tick 调用一次 App_Car_ProcessInputs，消费 ISR 入队字节并推进速率配置、静止确认和归零状态机。
 * @note 中断只负责接收入队；协议解析和 IMU 设备请求在本任务上下文运行，不分配内存。
 * @note 错过多个周期时只从当前 tick 重新排程；正常运行时永不返回，不阻塞等待 UART 数据。
 * @warning 本任务停摆会导致 IMU/JoyCon 数据老化；控制任务会因数据失效而制动，但急停仍依赖监控任务。
 */
void RTOS_UART_StartTask(void *argument);

/** @} */

#endif //LAYERED_EMBEDDED_C_RTOS_UART_H
