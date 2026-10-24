/**
 * @file platform_iwdg_common.c
 * @brief 与芯片系列无关的 IWDG 策略与状态管理。
 * @ingroup platform
 */

#include "platform_iwdg_backend.h"
#include <assert.h>
#include <stddef.h>

#define PLATFORM_IWDG_RELOAD_MAX 0x0FFFu

static const uint32_t s_iwdg_divisors[] = {
  4u,
  8u,
  16u,
  32u,
  64u,
  128u,
  256u,
};

static bool s_iwdg_started;

static bool platform_iwdg_calculate(
  uint32_t timeout_ms,
  uint32_t *divisor,
  uint32_t *reload
) {
  assert(divisor != NULL && reload != NULL);

  if (timeout_ms == 0U || timeout_ms > PLATFORM_IWDG_MAX_TIMEOUT_MS) {
    return false;
  }

  const uint64_t required_ticks =
    (uint64_t)timeout_ms * platform_iwdg_backend_lsi_hz() / 1000u;

  for (
    uint32_t i = 0U;
    i < sizeof(s_iwdg_divisors) / sizeof(s_iwdg_divisors[0]);
    i++
  ) {
    uint64_t loaded_ticks =
      (required_ticks + s_iwdg_divisors[i] - 1u) /
      s_iwdg_divisors[i];

    if (loaded_ticks <= (uint64_t)PLATFORM_IWDG_RELOAD_MAX + 1u) {
      *divisor = s_iwdg_divisors[i];
      *reload = (uint32_t)(loaded_ticks - 1u);
      return true;
    }
  }

  return false;
}

platform_status_t Platform_IWDG_Start(
  platform_iwdg_instance_t iwdg,
  uint32_t timeout_ms
) {
  if (iwdg != PLATFORM_IWDG_1) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (s_iwdg_started) {
    return PLATFORM_STATUS_BUSY;
  }

  uint32_t divisor;
  uint32_t reload;
  if (!platform_iwdg_calculate(timeout_ms, &divisor, &reload)) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  platform_status_t status =
    platform_iwdg_backend_start(divisor, reload);
  if (status == PLATFORM_STATUS_OK) {
    s_iwdg_started = true;
  }
  return status;
}

platform_status_t Platform_IWDG_Refresh(
  platform_iwdg_instance_t iwdg
) {
  if (iwdg != PLATFORM_IWDG_1) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (!s_iwdg_started) {
    return PLATFORM_STATUS_ERROR;
  }
  return platform_iwdg_backend_refresh();
}

bool Platform_IWDG_WasReset(platform_iwdg_instance_t iwdg) {
  if (iwdg != PLATFORM_IWDG_1) {
    return false;
  }
  return platform_iwdg_backend_was_reset();
}
