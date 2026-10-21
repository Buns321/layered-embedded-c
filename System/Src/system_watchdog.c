//
// Created by Buns_ on 2026/10/21.
//

#include "system_watchdog.h"
#include "system_critical.h"
#include <assert.h>

#if defined(STM32F4xx)
#include "stm32f4xx.h"
#elif defined(STM32F1xx)
#include "stm32f1xx.h"
#else
#error "Unsupported STM32 family"
#endif

#define SYS_WATCHDOG_KEY_START  0xCCCCu
#define SYS_WATCHDOG_KEY_UNLOCK 0x5555u
#define SYS_WATCHDOG_KEY_RELOAD 0xAAAAu

#define SYS_WATCHDOG_PRESCALER_32 3u
#define SYS_WATCHDOG_RELOAD_MAX   0x0FFFu

static uint32_t s_timeout_ms;
static uint32_t s_last_checkin[SYS_WATCHDOG_TASK_COUNT];
static uint32_t s_checkin_mask;

void Sys_Watchdog_Init(uint32_t timeout_ms) {
  assert(timeout_ms > 0u);

  if (timeout_ms == 0u) {
    timeout_ms = 1u;
  }

  if (timeout_ms > SYS_WATCHDOG_RELOAD_MAX) {
    timeout_ms = SYS_WATCHDOG_RELOAD_MAX;
  }

  s_timeout_ms = timeout_ms;

  for (uint32_t i = 0u; i < (uint32_t)SYS_WATCHDOG_TASK_COUNT; i++) {
    s_last_checkin[i] = 0u;
  }

  s_checkin_mask = 0u;

  IWDG->KR = SYS_WATCHDOG_KEY_START;
  IWDG->KR = SYS_WATCHDOG_KEY_UNLOCK;
  IWDG->PR = SYS_WATCHDOG_PRESCALER_32;
  IWDG->RLR = timeout_ms;

  while (IWDG->SR != 0u) {
  }

  IWDG->KR = SYS_WATCHDOG_KEY_RELOAD;
}

void Sys_Watchdog_CheckIn(Sys_Watchdog_Task task, uint32_t now_ms) {
  assert((uint32_t)task < (uint32_t)SYS_WATCHDOG_TASK_COUNT);

  if ((uint32_t)task < (uint32_t)SYS_WATCHDOG_TASK_COUNT) {
    Sys_Critical_Enter();
    s_last_checkin[task] = now_ms;
    s_checkin_mask |= (uint32_t)1u << (uint32_t)task;
    Sys_Critical_Exit();
  }
}

bool Sys_Watchdog_Service(uint32_t now_ms) {
  const uint32_t all_tasks_mask =
    ((uint32_t)1u << (uint32_t)SYS_WATCHDOG_TASK_COUNT) - 1u;

  uint32_t checkin_mask;
  uint32_t last_checkin[SYS_WATCHDOG_TASK_COUNT];

  Sys_Critical_Enter();
  checkin_mask = s_checkin_mask;
  for (uint32_t i = 0u; i < (uint32_t)SYS_WATCHDOG_TASK_COUNT; i++) {
    last_checkin[i] = s_last_checkin[i];
  }
  Sys_Critical_Exit();

  if ((checkin_mask & all_tasks_mask) != all_tasks_mask) {
    return false;
  }

  for (uint32_t i = 0u; i < (uint32_t)SYS_WATCHDOG_TASK_COUNT; i++) {
    if ((uint32_t)(now_ms - last_checkin[i]) > s_timeout_ms) {
      return false;
    }
  }

  IWDG->KR = SYS_WATCHDOG_KEY_RELOAD;
  return true;
}

bool Sys_Watchdog_WasReset(void) {
  bool was_reset = (RCC->CSR & RCC_CSR_IWDGRSTF) != 0u;

  if (was_reset) {
    RCC->CSR |= RCC_CSR_RMVF;
  }

  return was_reset;
}
