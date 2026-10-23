/**
 * @file platform_iwdg.c
 * @brief STM32F1 HAL backend for the common IWDG implementation.
 */

#include "platform_hal_internal.h"
#include "platform_iwdg_backend.h"
#include "stm32f1xx_hal.h"

#define PLATFORM_IWDG_LSI_HZ 40000u

static IWDG_HandleTypeDef s_iwdg_h;

static uint32_t platform_iwdg_hal_prescaler(uint32_t divisor) {
  switch (divisor) {
    case 4u:   return IWDG_PRESCALER_4;
    case 8u:   return IWDG_PRESCALER_8;
    case 16u:  return IWDG_PRESCALER_16;
    case 32u:  return IWDG_PRESCALER_32;
    case 64u:  return IWDG_PRESCALER_64;
    case 128u: return IWDG_PRESCALER_128;
    case 256u: return IWDG_PRESCALER_256;
    default:   return 0U;
  }
}

uint32_t platform_iwdg_backend_lsi_hz(void) {
  return PLATFORM_IWDG_LSI_HZ;
}

platform_status_t platform_iwdg_backend_start(
  uint32_t divisor,
  uint32_t reload
) {
  const uint32_t prescaler = platform_iwdg_hal_prescaler(divisor);
  if (prescaler == 0U) {
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
