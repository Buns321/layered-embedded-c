/**
 * @file sys_critical.c
 * @brief 任务与 ISR 临界区实现（统一经 bsp_mcu.h 接入芯片核心原语）。
 * @details 属于 Layer 2（Framework/Core），按 USE_FREERTOS 宏区分调度器启动前/后与裸机模式，
 *          所有嵌套深度与中断屏蔽状态均保存于静态变量中，零动态内存分配。
 * @ingroup Framework_Core
 */

#include "sys_critical.h"
#include "bsp_mcu.h"
#include <assert.h>

#ifdef USE_FREERTOS

#if defined(BSP_HOST_STUB)
#include "rtos_stub.h"
#else
#include "FreeRTOS.h"
#include "task.h"
#endif

/** @brief 调度器尚未启动阶段的任务临界区嵌套计数器。 */
static uint32_t s_boot_critical_nesting;

/** @brief 调度器尚未启动阶段进入首层临界区时保存的 PRIMASK 快照。 */
static uint32_t s_boot_primask;

void Sys_Critical_Enter(void) {
  // 调度器启动前不能调用 taskENTER_CRITICAL（可能触发 BASEPRI/vPortEnterCritical 状态异常），改用
  // PRIMASK 嵌套保护
  if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    // 仅在最外层（嵌套计数为 0）记录进入前的中断屏蔽状态，避免内层覆盖原始 PRIMASK
    if (s_boot_critical_nesting == 0U) {
      s_boot_primask = primask;
    }
    s_boot_critical_nesting++;
  } else {
    // 调度器启动后严禁残留未配对的启动期临界区，防止跨上下文破坏中断恢复状态
    configASSERT(s_boot_critical_nesting == 0U);
    taskENTER_CRITICAL();
  }
}

void Sys_Critical_Exit(void) {
  // 优先消耗启动期临界区嵌套计数；即使调度器刚启动，若仍处于启动期配对退出路径也能安全恢复 PRIMASK
  if (s_boot_critical_nesting > 0U) {
    s_boot_critical_nesting--;
    if (s_boot_critical_nesting == 0U) {
      __set_PRIMASK(s_boot_primask);
    }
  } else {
    taskEXIT_CRITICAL();
  }
}

sys_critical_state_t Sys_Critical_EnterFromISR(void) {
  // 委托给 FreeRTOS 的 BASEPRI 屏蔽原语，支持受控中断嵌套并返回原 BASEPRI 值
  return (sys_critical_state_t)taskENTER_CRITICAL_FROM_ISR();
}

void Sys_Critical_ExitFromISR(sys_critical_state_t state) {
  // 按进入时保存的 BASEPRI 快照原样恢复中断优先级阈值
  taskEXIT_CRITICAL_FROM_ISR((UBaseType_t)state);
}

#else

/** @brief 裸机模式下的临界区嵌套计数器。 */
static uint32_t s_critical_nesting;

/** @brief 裸机模式下进入最外层临界区时保存的 PRIMASK 快照。 */
static uint32_t s_critical_primask;

void Sys_Critical_Enter(void) {
  // 先读取进入前 PRIMASK 再关中断，避免读-改期间被高优先级中断篡改
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  if (s_critical_nesting == 0U) {
    s_critical_primask = primask;
  }
  s_critical_nesting++;
}

void Sys_Critical_Exit(void) {
  assert(s_critical_nesting > 0U);
  if (s_critical_nesting == 0U) {
    return;
  }

  s_critical_nesting--;
  // 仅当嵌套计数归零时恢复进入前 PRIMASK，维持原本可能已关中断的上下文
  if (s_critical_nesting == 0U) {
    __set_PRIMASK(s_critical_primask);
  }
}

sys_critical_state_t Sys_Critical_EnterFromISR(void) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return (sys_critical_state_t)primask;
}

void Sys_Critical_ExitFromISR(sys_critical_state_t state) { __set_PRIMASK((uint32_t)state); }

#endif /* USE_FREERTOS */
