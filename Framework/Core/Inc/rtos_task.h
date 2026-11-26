/**
 * @file rtos_task.h
 * @brief 静态周期 RTOS 任务声明、执行耗时/栈水位监控与栈溢出安全钩子。
 * @details 属于 Layer 2（Framework/Core），通过 RTOS_TASK_PERIODIC 宏声明 8 字节对齐
 *          静态栈与 StaticTask_t 控制块，完全禁用动态内存分配；并提供基于 DWT 的周期执行耗时
 *          与栈溢出紧急停机钩子机制。
 * @addtogroup Framework_Core
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
 * @brief 周期任务步进函数签名。
 * @param[in] now_ms 当前系统单调毫秒时间戳（单位：ms）。
 */
typedef void (*rtos_periodic_fn_t)(uint32_t now_ms);

/**
 * @brief 栈溢出等致命异常发生时的紧急停机回调签名。
 * @note 运行于异常/关中断临界上下文，禁止调用任何阻塞 API 或动态分配内存。
 */
typedef void (*rtos_emergency_halt_fn_t)(void);

/**
 * @brief 周期任务运行期统计信息。
 */
typedef struct rtos_task_stats_s {
  uint32_t last_exec_us;     /**< 最近一次步进函数执行耗时（单位：us）。 */
  uint32_t max_exec_us;      /**< 历史峰值步进函数执行耗时（单位：us）。 */
  uint32_t overrun_count;    /**< 累计周期超时或节拍滞后次数（单位：次）。 */
  uint32_t stack_free_bytes; /**< 最近一次采样的任务栈剩余可用空间（单位：B）。 */
} rtos_task_stats_t;

/**
 * @brief 静态分配的周期任务描述符（含 StaticTask_t 控制块与栈缓冲区引用）。
 */
typedef struct rtos_periodic_task_s {
  const char *name;           /**< 任务名称字符串（只读，用于注册与日志）。 */
  rtos_periodic_fn_t step_fn; /**< 每个周期触发一次的业务步进回调（不可为 NULL）。 */
  uint32_t period_ms;         /**< 目标调度周期（单位：ms，必须 > 0）。 */
  osPriority_t priority;      /**< CMSIS-RTOS2 任务优先级。 */
  StaticTask_t tcb;           /**< FreeRTOS 静态任务控制块（TCB）。 */
  uint32_t *stack_buf;        /**< 指向 8 字节对齐静态栈缓冲区的首地址。 */
  uint32_t stack_bytes;       /**< 静态栈缓冲区总字节数（单位：B）。 */
  osThreadId_t handle;        /**< osThreadNew() 返回的线程句柄（未启动时为 NULL）。 */
  rtos_task_stats_t stats;    /**< 任务运行期耗时、超时与栈水位统计快照。 */
} rtos_periodic_task_t;

/**
 * @brief 声明 8 字节对齐静态栈（uint64_t 数组）与静态周期任务控制块。
 * @param var_name      任务描述符静态变量名（同时用于生成 `<var_name>_stack_` 栈数组）。
 * @param period_ms_val 任务周期（单位：ms）。
 * @param prio_val      任务优先级（osPriority_t）。
 * @param stack_words   期望栈深度（单位：32 位字 / 4 B，向上取整至 8 字节对齐）。
 * @param fn            周期步进回调函数（类型为 rtos_periodic_fn_t）。
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
 * @brief 使用静态 TCB 与静态栈创建并启动周期任务。
 * @param[in,out] task 指向由 RTOS_TASK_PERIODIC 声明的静态周期任务描述符（不可为 NULL）。
 * @return 创建并注册成功与否。
 * @retval true  任务已成功创建并写入有效句柄。
 * @retval false 参数无效、任务已启动或 osThreadNew() 失败。
 * @pre 须在 osKernelInitialize() 完成且 osKernelStart() 启动前调用。
 */
bool RTOS_Task_StartPeriodic(rtos_periodic_task_t *task);

/**
 * @brief 在临界区内读取任务执行耗时、超时次数与栈剩余字节快照。
 * @param[in]  task 目标周期任务描述符指针（为 NULL 时安全返回）。
 * @param[out] out  接收统计信息快照的结构体指针（为 NULL 时安全返回）。
 * @note 仅限任务上下文调用，内部通过 Sys_Critical_Enter/Exit 保证多字段读取一致性。
 */
void RTOS_Task_GetStats(const rtos_periodic_task_t *task, rtos_task_stats_t *out);

/**
 * @brief 注册栈溢出紧急停机钩子（在 vApplicationStackOverflowHook 中优先执行）。
 * @param[in] hook 紧急停机回调函数指针（允许为 NULL 以清除钩子）。
 * @note 回调将在全局关中断状态下被调用，必须极简且直接操作硬件安全停机。
 */
void RTOS_SetEmergencyHaltHook(rtos_emergency_halt_fn_t hook);

/** @} */

#endif /* LAYERED_CAR_RTOS_TASK_H */
