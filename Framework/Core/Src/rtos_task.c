/**
 * @file rtos_task.c
 * @brief 静态周期 RTOS 任务运行循环、DWT 耗时监控与栈溢出安全钩子实现。
 * @ingroup sys_core
 */

#include "rtos_task.h"
#include "bsp_dwt.h"
#include "bsp_mcu.h"
#include "sys_critical.h"
#include "sys_log.h"
#include "sys_time_window.h"
#include <stddef.h>

#if !defined(BSP_HOST_STUB)
#include "task.h"
#endif

typedef struct rtos_task_loop_s {
  uint32_t tick_freq_hz;
  uint32_t period_ticks;
  uint32_t next_wake_tick;
  uint32_t last_stack_ms;
  bool stack_sampled;
} rtos_task_loop_t;

static rtos_emergency_halt_fn_t s_emergency_halt_hook;
static volatile bool s_stack_overflow_detected;
static TaskHandle_t volatile s_stack_overflow_task;

static uint32_t rtos_task_tickstoms(uint32_t ticks, uint32_t tick_freq_hz) {
  if (tick_freq_hz == 1000U) {
    return ticks;
  }
  return (uint32_t)(((uint64_t)ticks * 1000ULL) / (uint64_t)tick_freq_hz);
}

static uint32_t rtos_task_mstoticks(uint32_t period_ms, uint32_t tick_freq_hz) {
  uint64_t scaled = (uint64_t)period_ms * (uint64_t)tick_freq_hz;
  uint32_t ticks = (uint32_t)((scaled + 999ULL) / 1000ULL);
  return (ticks > 0U) ? ticks : 1U;
}

static void rtos_task_runstep(rtos_periodic_task_t *task, rtos_task_loop_t *loop) {
  uint32_t now_tick = osKernelGetTickCount();
  uint32_t lag_ticks = now_tick - loop->next_wake_tick;
  bool missed_period = (lag_ticks >= loop->period_ticks) && (lag_ticks < 0x80000000U);
  if (missed_period) {
    loop->next_wake_tick = now_tick;
  }
  loop->next_wake_tick += loop->period_ticks;

  uint32_t now_ms = rtos_task_tickstoms(now_tick, loop->tick_freq_hz);
  uint32_t start_cycles = BSP_DWT_GetCycles();
  task->step_fn(now_ms);
  uint32_t exec_us = BSP_DWT_CyclesToUs(BSP_DWT_GetCycles() - start_cycles);
  bool exec_overran = ((uint64_t)exec_us > ((uint64_t)task->period_ms * 1000ULL));

  bool sample_stack = false;
  uint32_t stack_free_bytes = 0U;
  if (!loop->stack_sampled || Sys_Time_HasElapsedMs(now_ms, loop->last_stack_ms, 1000U)) {
    stack_free_bytes = osThreadGetStackSpace(osThreadGetId());
    loop->last_stack_ms = now_ms;
    loop->stack_sampled = true;
    sample_stack = true;
  }

  Sys_Critical_Enter();
  task->stats.last_exec_us = exec_us;
  if (exec_us > task->stats.max_exec_us) {
    task->stats.max_exec_us = exec_us;
  }
  if (exec_overran || missed_period) {
    task->stats.overrun_count++;
  }
  if (sample_stack) {
    task->stats.stack_free_bytes = stack_free_bytes;
  }
  Sys_Critical_Exit();

  osStatus_t delay_st = osDelayUntil(loop->next_wake_tick);
  if (delay_st != osOK) {
    uint32_t after_tick = osKernelGetTickCount();
    if (Sys_Time_IsAfterMs(after_tick, loop->next_wake_tick)) {
      loop->next_wake_tick = after_tick;
      if (!exec_overran && !missed_period) {
        Sys_Critical_Enter();
        task->stats.overrun_count++;
        Sys_Critical_Exit();
      }
    }
  }
}

static void rtos_task_entry(void *argument) {
  rtos_periodic_task_t *task = (rtos_periodic_task_t *)argument;
  if (task == NULL || task->step_fn == NULL || task->period_ms == 0U) {
    return;
  }

  uint32_t tick_freq_hz = osKernelGetTickFreq();
  if (tick_freq_hz == 0U) {
    tick_freq_hz = 1000U;
  }

  uint32_t init_tick = osKernelGetTickCount();
  rtos_task_loop_t loop = {
      .tick_freq_hz = tick_freq_hz,
      .period_ticks = rtos_task_mstoticks(task->period_ms, tick_freq_hz),
      .next_wake_tick = init_tick,
      .last_stack_ms = rtos_task_tickstoms(init_tick, tick_freq_hz),
      .stack_sampled = false,
  };

  if (task->name != NULL) {
    SYS_LOG_INFO("RTOS", "%s started", task->name);
  }

#if defined(BSP_HOST_STUB)
  while (RTOS_Stub_ShouldContinueLoop()) {
    rtos_task_runstep(task, &loop);
  }
#else
  for (;;) {
    rtos_task_runstep(task, &loop);
  }
#endif
}

bool RTOS_Task_StartPeriodic(rtos_periodic_task_t *task) {
  if (task == NULL || task->step_fn == NULL || task->period_ms == 0U || task->stack_buf == NULL ||
      task->stack_bytes == 0U || task->handle != NULL) {
    return false;
  }

  task->stats = (rtos_task_stats_t){.last_exec_us = 0U};

  const osThreadAttr_t attr = {
    .name = task->name,
    .attr_bits = 0U,
    .cb_mem = &task->tcb,
    .cb_size = (uint32_t)sizeof(task->tcb),
    .stack_mem = task->stack_buf,
    .stack_size = task->stack_bytes,
    .priority = task->priority,
  };

  osThreadId_t handle = osThreadNew(rtos_task_entry, task, &attr);
  if (handle == NULL) {
    return false;
  }

  task->handle = handle;
  return true;
}

void RTOS_Task_GetStats(const rtos_periodic_task_t *task, rtos_task_stats_t *out) {
  if (task == NULL || out == NULL) {
    return;
  }

  Sys_Critical_Enter();
  *out = task->stats;
  Sys_Critical_Exit();
}

void RTOS_SetEmergencyHaltHook(rtos_emergency_halt_fn_t hook) {
  Sys_Critical_Enter();
  s_emergency_halt_hook = hook;
  Sys_Critical_Exit();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
  __disable_irq();
  s_stack_overflow_detected = true;
  s_stack_overflow_task = xTask;

  rtos_emergency_halt_fn_t hook = s_emergency_halt_hook;
  if (hook != NULL) {
    hook();
  }

  static const char k_prefix[] = "[RTOS] stack overflow: ";
  static const char k_unknown[] = "unknown";
  static const char k_suffix[] = "\r\n";

  /* 栈已溢出，使用零栈开销无锁直写接口，避免调用 snprintf 占用额外栈空间。 */
  Sys_Log_EmergencyRaw(k_prefix, (uint32_t)(sizeof(k_prefix) - 1U));
  if (pcTaskName != NULL) {
    uint32_t len = 0U;
    while (len < 32U && pcTaskName[len] != '\0') {
      len++;
    }
    if (len > 0U) {
      Sys_Log_EmergencyRaw(pcTaskName, len);
    } else {
      Sys_Log_EmergencyRaw(k_unknown, (uint32_t)(sizeof(k_unknown) - 1U));
    }
  } else {
    Sys_Log_EmergencyRaw(k_unknown, (uint32_t)(sizeof(k_unknown) - 1U));
  }
  Sys_Log_EmergencyRaw(k_suffix, (uint32_t)(sizeof(k_suffix) - 1U));

#if !defined(BSP_HOST_STUB)
  for (;;) {
    __disable_irq();
  }
#endif
}
