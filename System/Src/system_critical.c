//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file system_critical.c
 * @brief System 临界区实现。
 * @ingroup system
 */

#include "system_critical.h"
#include <assert.h>

#ifdef USE_FREERTOS

#include "FreeRTOS.h"
#include "task.h"
#if defined(STM32F4xx)
#include "stm32f4xx.h"
#elif defined(STM32F1xx)
#include "stm32f1xx.h"
#else
#error "Unsupported STM32 family"
#endif

static uint32_t s_boot_critical_nesting;
static uint32_t s_boot_primask;

/**
 * @brief 进入任务上下文临界区。
 * @pre 在任务上下文或调度器启动前调用；不得在 ISR 中使用。
 * @note 必须与 Sys_Critical_Exit 成对使用；FreeRTOS 构建在调度器启动前使用
 *       PRIMASK，启动后使用 taskENTER_CRITICAL。
 * @warning 只包住少量非阻塞操作；不得放串口、浮点三角函数或策略解算。
 * @post 当前上下文进入临界区，进入前的中断/抢占状态由模块保存。
 */
void Sys_Critical_Enter(void) {
  if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (s_boot_critical_nesting == 0u) {
      s_boot_primask = primask;
    }
    s_boot_critical_nesting++;
  } else {
    configASSERT(s_boot_critical_nesting == 0u);
    taskENTER_CRITICAL();
  }
}

/**
 * @brief 退出任务上下文临界区。
 * @pre 之前已调用 Sys_Critical_Enter，且本次 Enter 尚未配对退出。
 * @post 退出一层临界区；最外层退出后恢复对应的中断/抢占状态。
 * @note 必须与 Sys_Critical_Enter 成对使用；不分配动态内存。
 */
void Sys_Critical_Exit(void) {
  if (s_boot_critical_nesting > 0u) {
    s_boot_critical_nesting--;
    if (s_boot_critical_nesting == 0u) {
      __set_PRIMASK(s_boot_primask);
    }
  } else {
    taskEXIT_CRITICAL();
  }
}

/**
 * @brief 进入 ISR 上下文临界区。
 * @return 进入前的中断屏蔽状态快照，必须原样传给 Sys_Critical_ExitFromISR。
 * @pre 只能在 ISR 上下文调用。
 * @post 当前 ISR 进入临界区；返回的状态不得截断或复用给其他临界区。
 * @note 必须与 Sys_Critical_ExitFromISR 成对使用。
 */
sys_critical_state_t Sys_Critical_EnterFromISR(void) {
  return (sys_critical_state_t)taskENTER_CRITICAL_FROM_ISR();
}

/**
 * @brief 退出 ISR 上下文临界区并恢复中断状态。
 * @param state Sys_Critical_EnterFromISR 在同一 ISR 中返回的状态快照。
 * @pre state 必须来自同一 ISR 的 EnterFromISR，且尚未传给其他退出函数。
 * @post 恢复 state 保存的中断屏蔽状态。
 * @note 必须与 EnterFromISR 成对使用；不分配动态内存。
 */
void Sys_Critical_ExitFromISR(sys_critical_state_t state) {
  taskEXIT_CRITICAL_FROM_ISR((UBaseType_t)state);
}

#else

#if defined(STM32F4xx)
#include "stm32f4xx.h"
#elif defined(STM32F1xx)
#include "stm32f1xx.h"
#else
#error "Unsupported STM32 family"
#endif

static uint32_t s_critical_nesting;
static uint32_t s_critical_primask;

/**
 * @brief 进入任务上下文临界区。
 * @pre 在任务上下文或调度器启动前调用；不得在 ISR 中使用。
 * @note 必须与 Sys_Critical_Exit 成对使用；FreeRTOS 构建在调度器启动前使用
 *       PRIMASK，启动后使用 taskENTER_CRITICAL。
 * @warning 只包住少量非阻塞操作；不得放串口、浮点三角函数或策略解算。
 * @post 当前上下文进入临界区，进入前的中断/抢占状态由模块保存。
 */
void Sys_Critical_Enter(void) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  if (s_critical_nesting == 0u) {
    s_critical_primask = primask;
  }

  s_critical_nesting++;
}

/**
 * @brief 退出任务上下文临界区。
 * @pre 之前已调用 Sys_Critical_Enter，且本次 Enter 尚未配对退出。
 * @post 退出一层临界区；最外层退出后恢复对应的中断/抢占状态。
 * @note 必须与 Sys_Critical_Enter 成对使用；不分配动态内存。
 */
void Sys_Critical_Exit(void) {
  assert(s_critical_nesting > 0u);

  if (s_critical_nesting == 0u) {
    return;
  }

  s_critical_nesting--;

  if (s_critical_nesting == 0u) {
    __set_PRIMASK(s_critical_primask);
  }
}

/**
 * @brief 进入 ISR 上下文临界区。
 * @return 进入前的中断屏蔽状态快照，必须原样传给 Sys_Critical_ExitFromISR。
 * @pre 只能在 ISR 上下文调用。
 * @post 当前 ISR 进入临界区；返回的状态不得截断或复用给其他临界区。
 * @note 必须与 Sys_Critical_ExitFromISR 成对使用。
 */
sys_critical_state_t Sys_Critical_EnterFromISR(void) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return (sys_critical_state_t)primask;
}

/**
 * @brief 退出 ISR 上下文临界区并恢复中断状态。
 * @param state Sys_Critical_EnterFromISR 在同一 ISR 中返回的状态快照。
 * @pre state 必须来自同一 ISR 的 EnterFromISR，且尚未传给其他退出函数。
 * @post 恢复 state 保存的中断屏蔽状态。
 * @note 必须与 EnterFromISR 成对使用；不分配动态内存。
 */
void Sys_Critical_ExitFromISR(sys_critical_state_t state) {
  __set_PRIMASK((uint32_t)state);
}

#endif
