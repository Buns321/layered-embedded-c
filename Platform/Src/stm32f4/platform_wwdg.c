//
// 由 Buns_ 创建于 2026/10/22。
//

#include "platform_wwdg.h"
#include "platform_hal_internal.h"
#include "stm32f4xx_hal.h"
#include <assert.h>

#define PLATFORM_WWDG_COUNTER_START 0x7Fu
#define PLATFORM_WWDG_COUNTER_MIN   0x3Fu
#define PLATFORM_WWDG_PRESCALER_BASE 4096u

static WWDG_HandleTypeDef s_wwdg;
static bool s_wwdg_started;

static const uint32_t s_wwdg_divisors[] = {
  1u,
  2u,
  4u,
  8u,
};

static const uint32_t s_wwdg_prescalers[] = {
  WWDG_PRESCALER_1,
  WWDG_PRESCALER_2,
  WWDG_PRESCALER_4,
  WWDG_PRESCALER_8,
};

static bool platform_wwdg_calculate(
  uint32_t timeout_ms,
  uint32_t *prescaler
) {
  assert(prescaler != NULL);

  if (timeout_ms == 0U || timeout_ms > PLATFORM_WWDG_MAX_TIMEOUT_MS) {
    return false;
  }

  const uint64_t pclk1_hz = HAL_RCC_GetPCLK1Freq();
  const uint64_t required_cycles =
    (pclk1_hz / 1000u) * (uint64_t)timeout_ms;
  const uint64_t counter_steps =
    PLATFORM_WWDG_COUNTER_START - PLATFORM_WWDG_COUNTER_MIN;

  for (uint32_t i = 0U; i < sizeof(s_wwdg_divisors) / sizeof(s_wwdg_divisors[0]); i++) {
    uint64_t available_cycles =
      counter_steps *
      PLATFORM_WWDG_PRESCALER_BASE *
      s_wwdg_divisors[i];

    if (available_cycles >= required_cycles) {
      *prescaler = s_wwdg_prescalers[i];
      return true;
    }
  }

  return false;
}

Platform_StatusTypeDef Platform_WWDG_Start(
  Platform_WWDG_Instance wwdg,
  const Platform_WWDG_Config *config
) {
  assert(wwdg == PLATFORM_WWDG_1);
  assert(config != NULL);
  assert(
    config != NULL &&
    config->window >= PLATFORM_WWDG_WINDOW_MIN &&
    config->window <= PLATFORM_WWDG_WINDOW_MAX
  );

  if (
    wwdg != PLATFORM_WWDG_1 ||
    config == NULL ||
    config->window < PLATFORM_WWDG_WINDOW_MIN ||
    config->window > PLATFORM_WWDG_WINDOW_MAX
  ) {
    return Platform_ERROR;
  }

  assert(!s_wwdg_started);
  if (s_wwdg_started) {
    return Platform_BUSY;
  }

  uint32_t prescaler;
  if (!platform_wwdg_calculate(config->timeout_ms, &prescaler)) {
    return Platform_ERROR;
  }

  __HAL_RCC_WWDG_CLK_ENABLE();

  s_wwdg.Instance = WWDG;
  s_wwdg.Init.Prescaler = prescaler;
  s_wwdg.Init.Window = config->window;
  s_wwdg.Init.Counter = PLATFORM_WWDG_COUNTER_START;
  s_wwdg.Init.EWIMode = WWDG_EWI_DISABLE;

  Platform_StatusTypeDef status = platform_from_hal_status(
    HAL_WWDG_Init(&s_wwdg)
  );
  if (status == Platform_OK) {
    s_wwdg_started = true;
  }

  return status;
}

Platform_StatusTypeDef Platform_WWDG_Refresh(
  Platform_WWDG_Instance wwdg
) {
  assert(wwdg == PLATFORM_WWDG_1);
  if (wwdg != PLATFORM_WWDG_1 || !s_wwdg_started) {
    return Platform_ERROR;
  }

  return platform_from_hal_status(HAL_WWDG_Refresh(&s_wwdg));
}

bool Platform_WWDG_WasReset(Platform_WWDG_Instance wwdg) {
  assert(wwdg == PLATFORM_WWDG_1);
  (void)wwdg;

  bool was_reset = (RCC->CSR & RCC_CSR_WWDGRSTF) != 0U;
  if (was_reset) {
    RCC->CSR |= RCC_CSR_RMVF;
  }

  return was_reset;
}
