/**
 * @file platform_wwdg.c
 * @brief STM32F1 HAL backend for the common WWDG implementation.
 */

#include "platform_hal_internal.h"
#include "platform_wwdg_backend.h"
#include "stm32f1xx_hal.h"

#define PLATFORM_WWDG_COUNTER_START 0x7Fu

static WWDG_HandleTypeDef s_wwdg;

static uint32_t platform_wwdg_hal_prescaler(uint32_t divisor) {
  switch (divisor) {
    case 1u: return WWDG_PRESCALER_1;
    case 2u: return WWDG_PRESCALER_2;
    case 4u: return WWDG_PRESCALER_4;
    case 8u: return WWDG_PRESCALER_8;
    default: return 0U;
  }
}

uint32_t platform_wwdg_backend_pclk1_hz(void) {
  return HAL_RCC_GetPCLK1Freq();
}

Platform_StatusTypeDef platform_wwdg_backend_start(
  uint32_t divisor,
  uint8_t window
) {
  const uint32_t prescaler = platform_wwdg_hal_prescaler(divisor);
  if (prescaler == 0U) {
    return Platform_INVALID_ARGUMENT;
  }

  __HAL_RCC_WWDG_CLK_ENABLE();

  s_wwdg.Instance = WWDG;
  s_wwdg.Init.Prescaler = prescaler;
  s_wwdg.Init.Window = window;
  s_wwdg.Init.Counter = PLATFORM_WWDG_COUNTER_START;
  s_wwdg.Init.EWIMode = WWDG_EWI_DISABLE;
  return platform_from_hal_status(HAL_WWDG_Init(&s_wwdg));
}

Platform_StatusTypeDef platform_wwdg_backend_refresh(void) {
  return platform_from_hal_status(HAL_WWDG_Refresh(&s_wwdg));
}

bool platform_wwdg_backend_was_reset(void) {
  const bool was_reset = (RCC->CSR & RCC_CSR_WWDGRSTF) != 0U;
  if (was_reset) {
    RCC->CSR |= RCC_CSR_RMVF;
  }
  return was_reset;
}
