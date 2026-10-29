//
// 由 Buns_ 创建于 2026/8/31。
//

/**
 * @file rtos_monitor.c
 * @brief RTOS 监控任务实现。
 * @ingroup rtos
 */

#include "rtos_monitor.h"
#include "app_car.h"
#include "cmsis_os2.h"
#include "rtos_config.h"
#include "system_log.h"

/**
 * @brief 监控任务入口。
 * @note 参数为 CMSIS-RTOS2 任务参数，当前未使用。
 * @note 当前是最小占位实现，心跳和故障停机待接入。
 */
void RTOS_Monitor_StartTask(void *argument) {
  (void)argument;

  SYS_LOG_INFO("RTOS", "monitor task started");

  uint32_t next_wake_tick = osKernelGetTickCount();
  for (;;) {
    uint32_t now_tick = osKernelGetTickCount();
    if (
      (uint32_t)(now_tick - next_wake_tick) >
      RTOS_CAR_MONITOR_PERIOD_TICKS
    ) {
      next_wake_tick = now_tick;
    }
    next_wake_tick += RTOS_CAR_MONITOR_PERIOD_TICKS;

    App_Car_ProcessMonitor(now_tick);
    (void)osDelayUntil(next_wake_tick);
  }
}
