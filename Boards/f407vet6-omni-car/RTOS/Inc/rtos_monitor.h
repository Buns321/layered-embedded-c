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
 * @param argument 未使用。
 * @note 负责 50 Hz 遥测、1 Hz 状态日志、控制心跳和故障急停。
 */
void RTOS_Monitor_StartTask(void *argument);

/** @} */

#endif //LAYERED_EMBEDDED_C_RTOS_MONITOR_H
