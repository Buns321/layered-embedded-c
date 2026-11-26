/**
 * @file rtos_task.c
 * @brief 静态周期 RTOS 任务运行循环、DWT 耗时监控与栈溢出安全钩子实现。
 * @details 属于 Layer 2（Framework/Core），基于 osDelayUntil 实现无漂移绝对节拍调度，
 *          利用 DWT 周期计数器度量任务执行耗时，并强实现 FreeRTOS 栈溢出钩子以确保
 *          异常发生时立即切断硬件输出并输出零栈开销诊断信息。
 * @ingroup Framework_Core
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

/**
 * @brief 周期任务内部循环状态上下文（仅由对应任务线程独占访问）。
 */
typedef struct rtos_task_loop_s {
  uint32_t tick_freq_hz;   /**< RTOS 内核节拍频率（单位：Hz）。 */
  uint32_t period_ticks;   /**< 任务调度周期对应的内核节拍数（单位：tick）。 */
  uint32_t next_wake_tick; /**< 下一次目标唤醒的绝对节拍值（单位：tick）。 */
  uint32_t last_stack_ms;  /**< 上一次采样栈剩余空间的毫秒时间戳（单位：ms）。 */
  bool stack_sampled;      /**< 是否已完成至少一次栈剩余空间采样。 */
} rtos_task_loop_t;

/** @brief 栈溢出或致命异常发生时调用的紧急停机回调函数指针。 */
static rtos_emergency_halt_fn_t s_emergency_halt_hook;

/** @brief 栈溢出检测标志（用于调试器观察与状态锁存）。 */
static volatile bool s_stack_overflow_detected;

/** @brief 触发栈溢出的 FreeRTOS 任务句柄快照。 */
static TaskHandle_t volatile s_stack_overflow_task;

/**
 * @brief 将 RTOS 内核节拍数换算为毫秒时间戳。
 * @param[in] ticks        内核节拍数（单位：tick）。
 * @param[in] tick_freq_hz 内核节拍频率（单位：Hz，必须 > 0）。
 * @return 对应的毫秒时间戳（单位：ms）。
 */
static uint32_t rtos_task_tickstoms(uint32_t ticks, uint32_t tick_freq_hz) {
  if (tick_freq_hz == 1000U) {
    return ticks;
  }
  return (uint32_t)(((uint64_t)ticks * 1000ULL) / (uint64_t)tick_freq_hz);
}

/**
 * @brief 将毫秒周期向上取整换算为 RTOS 内核节拍数（至少 1 tick）。
 * @param[in] period_ms    期望周期（单位：ms）。
 * @param[in] tick_freq_hz 内核节拍频率（单位：Hz）。
 * @return 换算后的节拍数（单位：tick，最小返回 1）。
 */
static uint32_t rtos_task_mstoticks(uint32_t period_ms, uint32_t tick_freq_hz) {
  uint64_t scaled = (uint64_t)period_ms * (uint64_t)tick_freq_hz;
  uint32_t ticks = (uint32_t)((scaled + 999ULL) / 1000ULL);
  return (ticks > 0U) ? ticks : 1U;
}

/**
 * @brief 执行单次周期任务步进、统计耗时/栈水位并休眠至下一个绝对节拍。
 * @param[in,out] task 周期任务描述符指针。
 * @param[in,out] loop 任务内部调度循环状态指针。
 */
static void rtos_task_runstep(rtos_periodic_task_t *task, rtos_task_loop_t *loop) {
  uint32_t now_tick = osKernelGetTickCount();
  // 无符号减法自动处理 32 位 tick 回绕；lag_ticks < 0x80000000U 表示当前已越过预定唤醒点
  uint32_t lag_ticks = now_tick - loop->next_wake_tick;
  bool missed_period = (lag_ticks >= loop->period_ticks) && (lag_ticks < 0x80000000U);
  if (missed_period) {
    // 若任务被高优先级抢占或阻塞导致整周期丢失，重锚定基准节拍以防止连续追赶式空转
    loop->next_wake_tick = now_tick;
  }
  loop->next_wake_tick += loop->period_ticks;

  uint32_t now_ms = rtos_task_tickstoms(now_tick, loop->tick_freq_hz);
  uint32_t start_cycles = BSP_DWT_GetCycles();
  task->step_fn(now_ms);
  uint32_t exec_us = BSP_DWT_CyclesToUs(BSP_DWT_GetCycles() - start_cycles);
  bool exec_overran = ((uint64_t)exec_us > ((uint64_t)task->period_ms * 1000ULL));

  // 每 1000 ms 采样一次栈高水位（uxTaskGetStackHighWaterMark 开销较大，不宜每周期扫描）
  bool sample_stack = false;
  uint32_t stack_free_bytes = 0U;
  if (!loop->stack_sampled || Sys_Time_HasElapsedMs(now_ms, loop->last_stack_ms, 1000U)) {
    stack_free_bytes = osThreadGetStackSpace(osThreadGetId());
    loop->last_stack_ms = now_ms;
    loop->stack_sampled = true;
    sample_stack = true;
  }

  // 在临界区内原子更新统计结构，确保其他任务调用 RTOS_Task_GetStats 时读到一致快照
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
    // 若 osDelayUntil 因目标节拍已过期而未阻塞，检查是否确实滞后于唤醒点并重锚定
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

/**
 * @brief CMSIS-RTOS2 线程入口函数，初始化周期节拍状态并进入无限调度循环。
 * @param[in] argument 指向 rtos_periodic_task_t 描述符的不透明参数指针。
 */
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

  // 显式绑定静态 TCB 与 8 字节对齐静态栈缓冲区，杜绝 CMSIS-RTOS2 隐式动态分配
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

/**
 * @brief FreeRTOS 栈溢出钩子强实现（configCHECK_FOR_STACK_OVERFLOW=2）。
 * @param[in] xTask      发生栈溢出的任务句柄。
 * @param[in] pcTaskName 发生栈溢出的任务名称字符串（可能已被损坏或为 NULL）。
 * @note 立即全局关中断、执行紧急停机钩子并以零栈开销方式输出原始日志，随后死循环锁死。
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
  // 首要动作：全局关中断，防止调度器或中断继续驱动执行器
  __disable_irq();
  s_stack_overflow_detected = true;
  s_stack_overflow_task = xTask;

  // 若上层已注册硬件急停钩子，优先切断电机 PWM/STBY 等危险输出
  rtos_emergency_halt_fn_t hook = s_emergency_halt_hook;
  if (hook != NULL) {
    hook();
  }

  static const char k_prefix[] = "[RTOS] stack overflow: ";
  static const char k_unknown[] = "unknown";
  static const char k_suffix[] = "\r\n";

  // 栈已溢出，使用零栈开销无锁直写接口，避免调用 snprintf 占用额外栈空间并防止死锁
  Sys_Log_EmergencyRaw(k_prefix, (uint32_t)(sizeof(k_prefix) - 1U));
  if (pcTaskName != NULL) {
    // 限制最大扫描长度至 32 字节，防止 TCB 邻近内存损坏导致越界读
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
