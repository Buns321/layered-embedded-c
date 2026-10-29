/**
 * @file platform_wwdg.c
 * @brief STM32F4 公共 WWDG 实现的 HAL 后端。
 * @ingroup platform
 */

#include "platform_hal_internal.h"
#include "platform_wwdg_backend.h"
#include "stm32f4xx_hal.h"

#define PLATFORM_WWDG_COUNTER_START 0x7Fu

static WWDG_HandleTypeDef s_wwdg_h;

/**
 * @brief 把 WWDG 预分频值转换为 HAL 宏。
 */
static uint32_t platform_wwdg_hal_prescaler(uint32_t divisor) {
  switch (divisor) {
    case 1u: return WWDG_PRESCALER_1;
    case 2u: return WWDG_PRESCALER_2;
    case 4u: return WWDG_PRESCALER_4;
    case 8u: return WWDG_PRESCALER_8;
    default: return 0U;
  }
}

/**
 * @brief 返回窗口看门狗使用的 APB1 时钟频率。
 */
uint32_t platform_wwdg_backend_pclk1_hz(void) {
  return HAL_RCC_GetPCLK1Freq();
}

/**
 * @brief 启动窗口看门狗后端，不负责 CubeMX 结构性初始化。
 */
platform_status_t platform_wwdg_backend_start(
  uint32_t divisor,
  uint8_t window
) {
  const uint32_t prescaler = platform_wwdg_hal_prescaler(divisor);
  if (prescaler == 0U) {
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

/**
 * @brief 刷新窗口看门狗。
 */
platform_status_t platform_wwdg_backend_refresh(void) {
  return platform_from_hal_status(HAL_WWDG_Refresh(&s_wwdg_h));
}

/**
 * @brief 报告本次复位是否由窗口看门狗触发。
 */
bool platform_wwdg_backend_was_reset(void) {
  const bool was_reset = (RCC->CSR & RCC_CSR_WWDGRSTF) != 0U;
  if (was_reset) {
    RCC->CSR |= RCC_CSR_RMVF;
  }
  return was_reset;
}
