//
// 由 Buns_ 创建于 2026/8/31。
//

/**
 * @file rtos_control.c
 * @brief RTOS 小车控制任务实现。
 * @ingroup rtos
 */

#include "rtos_control.h"
#include "app_car.h"
#include "cmsis_os2.h"
#include "rtos_config.h"
#include "system_log.h"

/**
 * @brief 固定 100 Hz 的小车控制任务。
 * @param argument CMSIS-RTOS2 任务参数，当前未使用。
 * @note 任务入口不得阻塞；控制周期由 osDelayUntil 维护。
 */
void RTOS_Control_StartTask(void *argument) {
  (void)argument;

  SYS_LOG_INFO("RTOS", "control task started");

  uint32_t next_wake_tick = osKernelGetTickCount();
  for (;;) {
    uint32_t now_tick = osKernelGetTickCount();
    if (
      (uint32_t)(now_tick - next_wake_tick) >
      RTOS_CAR_CONTROL_PERIOD_TICKS
    ) {
      next_wake_tick = now_tick;
    }
    next_wake_tick += RTOS_CAR_CONTROL_PERIOD_TICKS;

    App_Car_ProcessControl(now_tick);
    (void)osDelayUntil(next_wake_tick);
  }
}
