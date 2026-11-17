/**
 * @file rtos_periodic.c
 * @brief CMSIS-RTOS2 固定周期任务循环实现。
 * @ingroup rtos
 */

#include "rtos_periodic.h"
#include "cmsis_os2.h"
#include "system_log.h"
#include <assert.h>
#include <stdint.h>

/**
 * @brief 把 tick 数转换为单调毫秒时间。
 * @param tick_count RTOS tick 数。
 * @param tick_freq_hz RTOS tick 频率，单位 Hz；必须大于 0。
 * @return 对应的毫秒时间戳；整数除法向下取整。
 * @pre tick_freq_hz 大于 0。
 * @post 不修改 RTOS 状态。
 * @note 当前频率为 1000 Hz 时直接返回 tick，避免每次执行除法。
 */
static uint32_t rtos_periodic_ticks_to_ms(
  uint32_t tick_count,
  uint32_t tick_freq_hz
) {
  assert(tick_freq_hz > 0u);
  if (tick_freq_hz == 1000u) {
    return tick_count;
  }
  return (uint32_t)(
    ((uint64_t)tick_count * 1000u) /
    tick_freq_hz
  );
}

/**
 * @brief 把毫秒周期向上取整为 RTOS tick 数。
 * @param period_ms 周期，单位 ms；必须大于 0。
 * @param tick_freq_hz RTOS tick 频率，单位 Hz；必须大于 0。
 * @return 非零 tick 数。
 * @pre period_ms 和 tick_freq_hz 均大于 0。
 * @post 不修改 RTOS 状态。
 * @note 向上取整避免配置周期短于一个 tick 时变成忙循环。
 */
static uint32_t rtos_periodic_ms_to_ticks(
  uint32_t period_ms,
  uint32_t tick_freq_hz
) {
  assert(period_ms > 0u && tick_freq_hz > 0u);
  uint64_t scaled = (uint64_t)period_ms * tick_freq_hz;
  uint32_t tick_count = (uint32_t)((scaled + 999u) / 1000u);
  return tick_count > 0u ? tick_count : 1u;
}

/**
 * @brief 运行一个固定周期 CMSIS-RTOS2 任务循环。
 * @param argument 指向只读 rtos_periodic_task_config_t 的静态存储。
 * @pre argument 非空，配置中的 process 非空，period_ms 大于 0。
 * @post 每个周期调用一次 process，并把 tick 转换为毫秒时间戳。
 * @note 本函数不返回；错过周期时从当前 tick 重新对齐，不补跑历史周期。
 * @warning 只适用于固定周期、无事件阻塞的任务。
 */
void RTOS_Periodic_RunTask(void *argument) {
  assert(argument);

  const rtos_periodic_task_config_t *config =
    (const rtos_periodic_task_config_t *)argument;
  assert(config->name && config->process && config->period_ms > 0u);

  uint32_t tick_freq_hz = osKernelGetTickFreq();
  uint32_t period_tick_count =
    rtos_periodic_ms_to_ticks(config->period_ms, tick_freq_hz);
  uint32_t next_wake_tick = osKernelGetTickCount();

  SYS_LOG_INFO("RTOS", "%s started", config->name);

  for (;;) {
    uint32_t now_tick = osKernelGetTickCount();
    if ((uint32_t)(now_tick - next_wake_tick) > period_tick_count) {
      next_wake_tick = now_tick;
    }
    next_wake_tick += period_tick_count;

    uint32_t now_ms = rtos_periodic_ticks_to_ms(now_tick, tick_freq_hz);
    config->process(now_ms);
    (void)osDelayUntil(next_wake_tick);
  }
}
