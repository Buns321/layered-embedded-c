/**
 * @file platform_iwdg.c
 * @brief STM32F1 公共 IWDG 实现的 HAL 后端。
 * @ingroup platform
 */

#include "platform_hal_internal.h"
#include "platform_iwdg_backend.h"
#include "stm32f1xx_hal.h"

#define PLATFORM_IWDG_LSI_HZ 40000u

static IWDG_HandleTypeDef s_iwdg_h;

static bool platform_iwdg_hal_prescaler(
  uint32_t divisor,
  uint32_t *prescaler
) {
  if (prescaler == NULL) {
    return false;
  }

  switch (divisor) {
    case 4u:   *prescaler = IWDG_PRESCALER_4; return true;
    case 8u:   *prescaler = IWDG_PRESCALER_8; return true;
    case 16u:  *prescaler = IWDG_PRESCALER_16; return true;
    case 32u:  *prescaler = IWDG_PRESCALER_32; return true;
    case 64u:  *prescaler = IWDG_PRESCALER_64; return true;
    case 128u: *prescaler = IWDG_PRESCALER_128; return true;
    case 256u: *prescaler = IWDG_PRESCALER_256; return true;
    default:   return false;
  }
}

uint32_t platform_iwdg_backend_lsi_hz(void) {
  return PLATFORM_IWDG_LSI_HZ;
}

platform_status_t platform_iwdg_backend_start(
  uint32_t divisor,
  uint32_t reload
) {
  uint32_t prescaler;
  if (!platform_iwdg_hal_prescaler(divisor, &prescaler)) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  s_iwdg_h.Instance = IWDG;
  s_iwdg_h.Init.Prescaler = prescaler;
  s_iwdg_h.Init.Reload = reload;
  return platform_from_hal_status(HAL_IWDG_Init(&s_iwdg_h));
}

platform_status_t platform_iwdg_backend_refresh(void) {
  return platform_from_hal_status(HAL_IWDG_Refresh(&s_iwdg_h));
}

bool platform_iwdg_backend_was_reset(void) {
  const bool was_reset = (RCC->CSR & RCC_CSR_IWDGRSTF) != 0U;
  if (was_reset) {
    RCC->CSR |= RCC_CSR_RMVF;
  }
  return was_reset;
}
