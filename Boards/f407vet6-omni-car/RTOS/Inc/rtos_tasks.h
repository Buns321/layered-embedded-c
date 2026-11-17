/**
 * @file rtos_tasks.h
 * @brief CubeMX 静态任务的 RTOS 运行入口。
 * @addtogroup rtos
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_RTOS_TASKS_H
#define LAYERED_EMBEDDED_C_RTOS_TASKS_H

/**
 * @brief 控制任务入口。
 * @param argument CMSIS-RTOS2 任务参数；当前忽略，允许为 NULL。
 * @pre CubeMX 已创建静态 ControlTask，App_Car_Init 已成功完成。
 * @post 永不返回，按配置周期调用 App_Car_ProcessControl。
 * @note 任务栈和 TCB 由 CubeMX 生成并静态分配。
 */
void RTOS_Control_StartTask(void *argument);

/**
 * @brief 输入任务入口。
 * @param argument CMSIS-RTOS2 任务参数；当前忽略，允许为 NULL。
 * @pre CubeMX 已创建静态 UARTTask，App_Car_Init 已成功完成。
 * @post 永不返回，按配置周期调用 App_Car_ProcessInputs。
 * @note 任务栈和 TCB 由 CubeMX 生成并静态分配。
 */
void RTOS_UART_StartTask(void *argument);

/**
 * @brief 监控任务入口。
 * @param argument CMSIS-RTOS2 任务参数；当前忽略，允许为 NULL。
 * @pre CubeMX 已创建静态 MonitorTask，App_Car_Init 已成功完成。
 * @post 永不返回，按配置周期调用 App_Car_ProcessMonitor。
 * @note 任务栈和 TCB 由 CubeMX 生成并静态分配。
 */
void RTOS_Monitor_StartTask(void *argument);

/** @} */

#endif //LAYERED_EMBEDDED_C_RTOS_TASKS_H
