/**
 * @file platform_iwdg.c
 * @brief STM32F4 公共 IWDG 实现的 HAL 后端。
 * @ingroup platform
 */

#include "platform_hal_internal.h"
#include "platform_iwdg_backend.h"
#include "stm32f4xx_hal.h"

#define PLATFORM_IWDG_LSI_HZ 32000u

static IWDG_HandleTypeDef s_iwdg_h;

/**
 * @brief 把 IWDG 预分频值转换为 HAL 宏。
 */
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

/**
 * @brief 返回独立看门狗使用的 LSI 频率。
 */
uint32_t platform_iwdg_backend_lsi_hz(void) {
  return PLATFORM_IWDG_LSI_HZ;
}

/**
 * @brief 启动独立看门狗后端，不负责 CubeMX 结构性初始化。
 */
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

/**
 * @brief 刷新独立看门狗。
 */
platform_status_t platform_iwdg_backend_refresh(void) {
  return platform_from_hal_status(HAL_IWDG_Refresh(&s_iwdg_h));
}

/**
 * @brief 报告本次复位是否由独立看门狗触发。
 */
bool platform_iwdg_backend_was_reset(void) {
  const bool was_reset = (RCC->CSR & RCC_CSR_IWDGRSTF) != 0U;
  if (was_reset) {
    RCC->CSR |= RCC_CSR_RMVF;
  }
  return was_reset;
}
