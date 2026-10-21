//
// Created by Buns_ on 2026/10/22.
//

#include "platform_iwdg.h"
#include "platform_hal_internal.h"
#include "stm32f1xx_hal.h"
#include <assert.h>

#define PLATFORM_IWDG_LSI_HZ        40000u
#define PLATFORM_IWDG_RELOAD_MAX    0x0FFFu

static IWDG_HandleTypeDef s_iwdg;
static bool s_iwdg_started;

static const uint32_t s_iwdg_divisors[] = {
  4u,
  8u,
  16u,
  32u,
  64u,
  128u,
  256u,
};

static const uint32_t s_iwdg_prescalers[] = {
  IWDG_PRESCALER_4,
  IWDG_PRESCALER_8,
  IWDG_PRESCALER_16,
  IWDG_PRESCALER_32,
  IWDG_PRESCALER_64,
  IWDG_PRESCALER_128,
  IWDG_PRESCALER_256,
};

static bool platform_iwdg_calculate(
  uint32_t timeout_ms,
  uint32_t *prescaler,
  uint32_t *reload
) {
  assert(prescaler != NULL && reload != NULL);

  if (timeout_ms == 0U || timeout_ms > PLATFORM_IWDG_MAX_TIMEOUT_MS) {
    return false;
  }

  const uint64_t required_ticks =
    (uint64_t)timeout_ms * PLATFORM_IWDG_LSI_HZ / 1000u;

  for (uint32_t i = 0U; i < sizeof(s_iwdg_divisors) / sizeof(s_iwdg_divisors[0]); i++) {
    uint64_t loaded_ticks =
      (required_ticks + s_iwdg_divisors[i] - 1u) /
      s_iwdg_divisors[i];

    if (loaded_ticks <= (uint64_t)PLATFORM_IWDG_RELOAD_MAX + 1u) {
      *prescaler = s_iwdg_prescalers[i];
      *reload = (uint32_t)(loaded_ticks - 1u);
      return true;
    }
  }

  return false;
}

Platform_StatusTypeDef Platform_IWDG_Start(
  Platform_IWDG_Instance iwdg,
  uint32_t timeout_ms
) {
  assert(iwdg == PLATFORM_IWDG_1);
  if (iwdg != PLATFORM_IWDG_1) {
    return Platform_ERROR;
  }

  assert(!s_iwdg_started);
  if (s_iwdg_started) {
    return Platform_BUSY;
  }

  uint32_t prescaler;
  uint32_t reload;
  if (!platform_iwdg_calculate(timeout_ms, &prescaler, &reload)) {
    return Platform_ERROR;
  }

  s_iwdg.Instance = IWDG;
  s_iwdg.Init.Prescaler = prescaler;
  s_iwdg.Init.Reload = reload;

  Platform_StatusTypeDef status = platform_from_hal_status(
    HAL_IWDG_Init(&s_iwdg)
  );
  s_iwdg_started = true;
  return status;
}

Platform_StatusTypeDef Platform_IWDG_Refresh(
  Platform_IWDG_Instance iwdg
) {
  assert(iwdg == PLATFORM_IWDG_1);
  if (iwdg != PLATFORM_IWDG_1 || !s_iwdg_started) {
    return Platform_ERROR;
  }

  return platform_from_hal_status(HAL_IWDG_Refresh(&s_iwdg));
}

bool Platform_IWDG_WasReset(Platform_IWDG_Instance iwdg) {
  assert(iwdg == PLATFORM_IWDG_1);
  (void)iwdg;

  bool was_reset = (RCC->CSR & RCC_CSR_IWDGRSTF) != 0U;
  if (was_reset) {
    RCC->CSR |= RCC_CSR_RMVF;
  }

  return was_reset;
}
