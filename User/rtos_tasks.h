/**
 * @file rtos_tasks.h
 * @brief F407 静态周期任务集中启动入口。
 * @details 封装 CMSIS-RTOS2 静态任务创建与启动，对 Core/Src/freertos.c
 *          仅暴露单一无参启动接口，保持应用逻辑与调度配置解耦。
 *
 * @defgroup User_RTOS 静态周期任务调度
 * @{
 */

#ifndef LAYERED_CAR_USER_RTOS_TASKS_H
#define LAYERED_CAR_USER_RTOS_TASKS_H

#include <stdbool.h>

/**
 * @brief 创建并启动全部静态周期任务。
 * @return 所有静态任务是否均创建成功。
 * @retval true  控制、输入、服务三个静态周期任务全部启动成功。
 * @retval false 任一任务创建失败（例如优先级或参数无效）。
 * @pre 必须在 osKernelInitialize() 之后、osKernelStart() 之前调用，
 *      且 App_Car_Boot() 须已完成整车初始化。
 * @note 所有任务 TCB 与 8 字节对齐栈均静态分配，不消耗 FreeRTOS 堆内存。
 */
bool RTOS_StartAllTasks(void);

#endif /* LAYERED_CAR_USER_RTOS_TASKS_H */

/** @} */
