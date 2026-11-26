/**
 * @file rtos_tasks.h
 * @brief F407 静态周期任务集中启动入口。
 */

#ifndef LAYERED_CAR_USER_RTOS_TASKS_H
#define LAYERED_CAR_USER_RTOS_TASKS_H

#include <stdbool.h>

/** @brief 创建并启动全部静态周期任务（须在 osKernelInitialize 后调用）。 */
bool RTOS_StartAllTasks(void);

#endif /* LAYERED_CAR_USER_RTOS_TASKS_H */
