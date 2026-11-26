/**
 * @file rtos_task.h
 * @brief 静态周期 RTOS 任务声明、执行耗时/栈水位监控与栈溢出安全钩子。
 * @addtogroup sys_core
 * @{
 */

#ifndef LAYERED_CAR_RTOS_TASK_H
#define LAYERED_CAR_RTOS_TASK_H

#include <stdbool.h>
#include <stdint.h>

#if defined(BSP_HOST_STUB)
#include "rtos_stub.h"
#else
#include "FreeRTOS.h"
#include "cmsis_os2.h"
#endif

/**
 * @brief 周期任务步进函数签名（now_ms 为单调毫秒时间戳，单位 ms）。
 */
typedef void (*rtos_periodic_fn_t)(uint32_t now_ms);

/**
 * @brief 栈溢出等致命异常发生时的紧急停机回调签名（ISR/临界上下文调用，禁止阻塞）。
 */
typedef void (*rtos_emergency_halt_fn_t)(void);

/**
 * @brief 周期任务运行期统计信息。
 */
typedef struct rtos_task_stats_s {
  uint32_t last_exec_us;
  uint32_t max_exec_us;
  uint32_t overrun_count;
  uint32_t stack_free_bytes;
} rtos_task_stats_t;

/**
 * @brief 静态分配的周期任务描述符（含 StaticTask_t 控制块与栈缓冲区引用）。
 */
typedef struct rtos_periodic_task_s {
  const char *name;
  rtos_periodic_fn_t step_fn;
  uint32_t period_ms;
  osPriority_t priority;
  StaticTask_t tcb;
  uint32_t *stack_buf;
  uint32_t stack_bytes;
  osThreadId_t handle;
  rtos_task_stats_t stats;
} rtos_periodic_task_t;

/**
 * @brief 声明 8 字节对齐静态栈（uint64_t 数组）与静态周期任务控制块。
 */
#define RTOS_TASK_PERIODIC(var_name, period_ms_val, prio_val, stack_words, fn)                     \
  static uint64_t var_name##_stack_[(((stack_words) + 1U) / 2U)];                                  \
  static rtos_periodic_task_t var_name = {                                                         \
      .name = #var_name,                                                                           \
      .step_fn = (fn),                                                                             \
      .period_ms = (period_ms_val),                                                                \
      .priority = (prio_val),                                                                      \
      .stack_buf = (uint32_t *)var_name##_stack_,                                                  \
      .stack_bytes = (uint32_t)sizeof(var_name##_stack_),                                          \
  }

/**
 * @brief 使用静态 TCB 与静态栈创建并启动周期任务（须在 osKernelInitialize 后调用）。
 */
bool RTOS_Task_StartPeriodic(rtos_periodic_task_t *task);

/**
 * @brief 在临界区内读取任务执行耗时、超时次数与栈剩余字节快照。
 */
void RTOS_Task_GetStats(const rtos_periodic_task_t *task, rtos_task_stats_t *out);

/**
 * @brief 注册栈溢出紧急停机钩子（在 vApplicationStackOverflowHook 中优先执行）。
 */
void RTOS_SetEmergencyHaltHook(rtos_emergency_halt_fn_t hook);

/** @} */

#endif /* LAYERED_CAR_RTOS_TASK_H */
