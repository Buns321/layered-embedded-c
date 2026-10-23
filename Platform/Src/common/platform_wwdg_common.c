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

uint32_t Platform_WWDG_GetMaxTimeoutMs(Platform_WWDG_Instance wwdg) {
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

Platform_StatusTypeDef Platform_WWDG_Start(
  Platform_WWDG_Instance wwdg,
  const Platform_WWDG_Config *config
) {
  if (
    wwdg != PLATFORM_WWDG_1 ||
    config == NULL ||
    config->window < PLATFORM_WWDG_WINDOW_MIN ||
    config->window > PLATFORM_WWDG_WINDOW_MAX
  ) {
    return Platform_INVALID_ARGUMENT;
  }
  if (s_wwdg_started) {
    return Platform_BUSY;
  }

  uint32_t divisor;
  if (!platform_wwdg_calculate(config->timeout_ms, &divisor)) {
    return Platform_INVALID_ARGUMENT;
  }

  Platform_StatusTypeDef status =
    platform_wwdg_backend_start(divisor, config->window);
  if (status == Platform_OK) {
    s_wwdg_started = true;
  }
  return status;
}

Platform_StatusTypeDef Platform_WWDG_Refresh(
  Platform_WWDG_Instance wwdg
) {
  if (wwdg != PLATFORM_WWDG_1) {
    return Platform_INVALID_ARGUMENT;
  }
  if (!s_wwdg_started) {
    return Platform_ERROR;
  }
  return platform_wwdg_backend_refresh();
}

bool Platform_WWDG_WasReset(Platform_WWDG_Instance wwdg) {
  if (wwdg != PLATFORM_WWDG_1) {
    return false;
  }
  return platform_wwdg_backend_was_reset();
}
