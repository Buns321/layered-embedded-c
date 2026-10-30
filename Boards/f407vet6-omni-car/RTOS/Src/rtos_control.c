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
 * @brief 固定 100 Hz 的小车控制任务入口。
 * @param argument CMSIS-RTOS2 任务参数；当前忽略，允许为 NULL。
 * @pre 仅在调度器启动后由内核调用，且 App_Car_Init 已成功完成。
 * @post 每个 10 tick 周期调用一次 App_Car_ProcessControl；错过多个周期时只从当前 tick 重新排程，不补跑历史周期。
 * @note 任务通过 osDelayUntil 保持 100 Hz；正常运行时永不返回，不分配内存，也不阻塞等待应用事件。
 * @warning 本任务负责四轮闭环和故障急停，长时间被更高优先级任务抢占会触发心跳超时。
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
