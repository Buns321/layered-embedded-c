/**
 * @file sys_critical.c
 * @brief 任务与 ISR 临界区实现（统一经 bsp_mcu.h 接入芯片核心原语）。
 * @ingroup sys_core
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

static uint32_t s_boot_critical_nesting;
static uint32_t s_boot_primask;

void Sys_Critical_Enter(void) {
  if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (s_boot_critical_nesting == 0U) {
      s_boot_primask = primask;
    }
    s_boot_critical_nesting++;
  } else {
    configASSERT(s_boot_critical_nesting == 0U);
    taskENTER_CRITICAL();
  }
}

void Sys_Critical_Exit(void) {
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
  return (sys_critical_state_t)taskENTER_CRITICAL_FROM_ISR();
}

void Sys_Critical_ExitFromISR(sys_critical_state_t state) {
  taskEXIT_CRITICAL_FROM_ISR((UBaseType_t)state);
}

#else

static uint32_t s_critical_nesting;
static uint32_t s_critical_primask;

void Sys_Critical_Enter(void) {
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
