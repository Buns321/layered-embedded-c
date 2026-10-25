/**
 * @file platform_wwdg.c
 * @brief STM32F1 公共 WWDG 实现的 HAL 后端。
 * @ingroup platform
 */

#include "platform_hal_internal.h"
#include "platform_wwdg_backend.h"
#include "stm32f1xx_hal.h"

#define PLATFORM_WWDG_COUNTER_START 0x7Fu

static WWDG_HandleTypeDef s_wwdg_h;

static bool platform_wwdg_hal_prescaler(
  uint32_t divisor,
  uint32_t *prescaler
) {
  if (prescaler == NULL) {
    return false;
  }

  switch (divisor) {
    case 1u: *prescaler = WWDG_PRESCALER_1; return true;
    case 2u: *prescaler = WWDG_PRESCALER_2; return true;
    case 4u: *prescaler = WWDG_PRESCALER_4; return true;
    case 8u: *prescaler = WWDG_PRESCALER_8; return true;
    default: return false;
  }
}

uint32_t platform_wwdg_backend_pclk1_hz(void) {
  return HAL_RCC_GetPCLK1Freq();
}

platform_status_t platform_wwdg_backend_start(
  uint32_t divisor,
  uint8_t window
) {
  uint32_t prescaler;
  if (!platform_wwdg_hal_prescaler(divisor, &prescaler)) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  __HAL_RCC_WWDG_CLK_ENABLE();

  s_wwdg_h.Instance = WWDG;
  s_wwdg_h.Init.Prescaler = prescaler;
  s_wwdg_h.Init.Window = window;
  s_wwdg_h.Init.Counter = PLATFORM_WWDG_COUNTER_START;
  s_wwdg_h.Init.EWIMode = WWDG_EWI_DISABLE;
  return platform_from_hal_status(HAL_WWDG_Init(&s_wwdg_h));
}

platform_status_t platform_wwdg_backend_refresh(void) {
  return platform_from_hal_status(HAL_WWDG_Refresh(&s_wwdg_h));
}

bool platform_wwdg_backend_was_reset(void) {
  const bool was_reset = (RCC->CSR & RCC_CSR_WWDGRSTF) != 0U;
  if (was_reset) {
    RCC->CSR |= RCC_CSR_RMVF;
  }
  return was_reset;
}
