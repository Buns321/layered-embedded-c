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
 * @brief 固定 100 Hz 的监控任务入口。
 * @param argument CMSIS-RTOS2 任务参数；当前忽略，允许为 NULL。
 * @pre 仅在调度器启动后由内核调用，且 App_Car_Init 已成功完成。
 * @post 每 10 tick 调用一次 App_Car_ProcessMonitor，由其执行心跳检查、50 Hz 遥测和 1 Hz 状态日志。
 * @note 遥测和日志分别按 20 ms、1000 ms 在应用层节流；任务本身仍为 100 Hz，正常运行时永不返回。
 * @note 错过多个调度周期时只从当前 tick 重新排程；不分配内存，不等待 DMA 或应用事件。
 * @warning 控制心跳超时只在本任务获得 CPU 后才会锁存；监控任务长期得不到调度时无法保证及时急停。
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
