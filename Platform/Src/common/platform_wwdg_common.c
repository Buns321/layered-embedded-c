/**
 * @file platform_wwdg_common.c
 * @brief Family-independent WWDG policy and state management.
 */

#include "platform_wwdg_backend.h"
#include <assert.h>
#include <stddef.h>

#define PLATFORM_WWDG_COUNTER_START 0x7Fu
#define PLATFORM_WWDG_COUNTER_MIN   0x3Fu
#define PLATFORM_WWDG_PRESCALER_BASE 4096u

static const uint32_t s_wwdg_divisors[] = {
  1u,
  2u,
  4u,
  8u,
};

static bool s_wwdg_started;

uint32_t Platform_WWDG_GetMaxTimeoutMs(platform_wwdg_instance_t wwdg) {
  if (wwdg != PLATFORM_WWDG_1) {
    return 0U;
  }

  const uint64_t pclk1_hz = platform_wwdg_backend_pclk1_hz();
  if (pclk1_hz == 0U) {
    return 0U;
  }

  const uint64_t counter_steps =
    PLATFORM_WWDG_COUNTER_START - PLATFORM_WWDG_COUNTER_MIN;
  const uint64_t max_cycles =
    counter_steps *
    PLATFORM_WWDG_PRESCALER_BASE *
    s_wwdg_divisors[
      sizeof(s_wwdg_divisors) / sizeof(s_wwdg_divisors[0]) - 1u
    ];

  return (uint32_t)((max_cycles * 1000U) / pclk1_hz);
}

static bool platform_wwdg_calculate(
  uint32_t timeout_ms,
  uint32_t *divisor
) {
  assert(divisor != NULL);

  if (
    timeout_ms == 0U ||
    timeout_ms > Platform_WWDG_GetMaxTimeoutMs(PLATFORM_WWDG_1)
  ) {
    return false;
  }

  const uint64_t pclk1_hz = platform_wwdg_backend_pclk1_hz();
  const uint64_t required_cycles =
    (pclk1_hz / 1000u) * (uint64_t)timeout_ms;
  const uint64_t counter_steps =
    PLATFORM_WWDG_COUNTER_START - PLATFORM_WWDG_COUNTER_MIN;

  for (
    uint32_t i = 0U;
    i < sizeof(s_wwdg_divisors) / sizeof(s_wwdg_divisors[0]);
    i++
  ) {
    uint64_t available_cycles =
      counter_steps *
      PLATFORM_WWDG_PRESCALER_BASE *
      s_wwdg_divisors[i];

    if (available_cycles >= required_cycles) {
      *divisor = s_wwdg_divisors[i];
      return true;
    }
  }

  return false;
}

platform_status_t Platform_WWDG_Start(
  platform_wwdg_instance_t wwdg,
  const platform_wwdg_config_t *config
) {
  if (
    wwdg != PLATFORM_WWDG_1 ||
    config == NULL ||
    config->window < PLATFORM_WWDG_WINDOW_MIN ||
    config->window > PLATFORM_WWDG_WINDOW_MAX
  ) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (s_wwdg_started) {
    return PLATFORM_STATUS_BUSY;
  }

  uint32_t divisor;
  if (!platform_wwdg_calculate(config->timeout_ms, &divisor)) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  platform_status_t status =
    platform_wwdg_backend_start(divisor, config->window);
  if (status == PLATFORM_STATUS_OK) {
    s_wwdg_started = true;
  }
  return status;
}

platform_status_t Platform_WWDG_Refresh(
  platform_wwdg_instance_t wwdg
) {
  if (wwdg != PLATFORM_WWDG_1) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (!s_wwdg_started) {
    return PLATFORM_STATUS_ERROR;
  }
  return platform_wwdg_backend_refresh();
}

bool Platform_WWDG_WasReset(platform_wwdg_instance_t wwdg) {
  if (wwdg != PLATFORM_WWDG_1) {
    return false;
  }
  return platform_wwdg_backend_was_reset();
}
