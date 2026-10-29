//
// 由 Buns_ 创建于 2026/8/31。
//

/**
 * @file rtos_monitor.c
 * @brief RTOS 监控任务实现。
 * @ingroup rtos
 */

#include "rtos_monitor.h"

#include "cmsis_os2.h"

/**
 * @brief 监控任务入口。
 * @note 参数为 CMSIS-RTOS2 任务参数，当前未使用。
 * @note 当前是最小占位实现，心跳和故障停机待接入。
 */
void RTOS_Monitor_StartTask(void *argument) {
  (void)argument;
  for (;;) {
    osDelay(100);
  }
}
