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
 * @note 必须与 Sys_Critical_Exit 成对使用。
 * @note 调度器未启动时使用 PRIMASK，启动后使用 FreeRTOS 任务临界区。
 * @warning 只包住几条指令；不要放阻塞调用、串口、浮点三角函数或策略解算。
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
 * @return 进入前的中断状态，供退出时恢复。
 * @note 必须与 Sys_Critical_ExitFromISR 成对使用。
 */
sys_critical_state_t Sys_Critical_EnterFromISR(void) {
  return (sys_critical_state_t)taskENTER_CRITICAL_FROM_ISR();
}

/**
 * @brief 退出 ISR 上下文临界区并恢复中断状态。
 * @param state Sys_Critical_EnterFromISR 返回的状态。
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
 * @note 必须与 Sys_Critical_Exit 成对使用。
 * @note 调度器未启动时使用 PRIMASK，启动后使用 FreeRTOS 任务临界区。
 * @warning 只包住几条指令；不要放阻塞调用、串口、浮点三角函数或策略解算。
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
 * @return 进入前的中断状态，供退出时恢复。
 * @note 必须与 Sys_Critical_ExitFromISR 成对使用。
 */
sys_critical_state_t Sys_Critical_EnterFromISR(void) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return (sys_critical_state_t)primask;
}

/**
 * @brief 退出 ISR 上下文临界区并恢复中断状态。
 * @param state Sys_Critical_EnterFromISR 返回的状态。
 */
void Sys_Critical_ExitFromISR(sys_critical_state_t state) {
  __set_PRIMASK((uint32_t)state);
}

#endif
