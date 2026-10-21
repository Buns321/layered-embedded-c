//
// Created by Buns_ on 2026/10/21.
//

#include "system_critical.h"
#include <assert.h>

#ifdef USE_FREERTOS

#include "FreeRTOS.h"
#include "task.h"

void Sys_Critical_Enter(void) {
  taskENTER_CRITICAL();
}

void Sys_Critical_Exit(void) {
  taskEXIT_CRITICAL();
}

Sys_Critical_State Sys_Critical_EnterFromISR(void) {
  return (Sys_Critical_State)taskENTER_CRITICAL_FROM_ISR();
}

void Sys_Critical_ExitFromISR(Sys_Critical_State state) {
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

void Sys_Critical_Enter(void) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  if (s_critical_nesting == 0u) {
    s_critical_primask = primask;
  }

  s_critical_nesting++;
}

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

Sys_Critical_State Sys_Critical_EnterFromISR(void) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return (Sys_Critical_State)primask;
}

void Sys_Critical_ExitFromISR(Sys_Critical_State state) {
  __set_PRIMASK((uint32_t)state);
}

#endif
