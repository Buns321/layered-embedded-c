//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file system_watchdog.c
 * @brief System 看门狗策略实现。
 * @ingroup system
 */

#include "system_watchdog.h"
#include "platform_iwdg.h"
#include "system_critical.h"
#include <assert.h>

static uint32_t s_timeout_ms;
static uint32_t s_last_checkin[SYS_WATCHDOG_TASK_COUNT];
static uint32_t s_checkin_mask;

void Sys_Watchdog_Init(uint32_t timeout_ms) {
  assert(timeout_ms > 0u);

  if (timeout_ms == 0u) {
    timeout_ms = 1u;
  }

  if (timeout_ms > PLATFORM_IWDG_MAX_TIMEOUT_MS) {
    timeout_ms = PLATFORM_IWDG_MAX_TIMEOUT_MS;
  }

  s_timeout_ms = timeout_ms;

  for (uint32_t i = 0u; i < (uint32_t)SYS_WATCHDOG_TASK_COUNT; i++) {
    s_last_checkin[i] = 0u;
  }

  s_checkin_mask = 0u;

  platform_status_t status = Platform_IWDG_Start(
    PLATFORM_IWDG_1,
    timeout_ms
  );
  assert(status == PLATFORM_STATUS_OK);
}

void Sys_Watchdog_CheckIn(sys_watchdog_task_t task, uint32_t now_ms) {
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

  return Platform_IWDG_Refresh(PLATFORM_IWDG_1) == PLATFORM_STATUS_OK;
}

bool Sys_Watchdog_WasReset(void) {
  return Platform_IWDG_WasReset(PLATFORM_IWDG_1);
}
