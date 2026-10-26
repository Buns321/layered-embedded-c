//
// 由 Buns_ 创建于 2026/8/31。
//

/**
 * @file rtos_monitor.h
 * @brief 监控任务接口。
 * @addtogroup rtos
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_RTOS_MONITOR_H
#define LAYERED_EMBEDDED_C_RTOS_MONITOR_H

/**
 * @brief 监控任务入口。
 * @note 参数为 CMSIS-RTOS2 任务参数，当前未使用。
 * @note 当前是最小占位实现，心跳和故障停机待接入。
 */
void RTOS_Monitor_StartTask(void *argument);

/** @} */

#endif //LAYERED_EMBEDDED_C_RTOS_MONITOR_H
