/**
 * @file stm32f1_watchdog_backend_test.c
 * @brief STM32F1 IWDG/WWDG HAL 分频编码回归测试。
 * @ingroup tests
 */

#include "platform_iwdg_backend.h"
#include "platform_wwdg_backend.h"
#include "stm32f1xx_hal.h"

#include <assert.h>
#include <stdint.h>

RCC_Registers g_stm32f1_test_rcc;

static uint32_t s_iwdg_prescaler;
static uint32_t s_iwdg_reload;
static uint32_t s_wwdg_prescaler;
static uint32_t s_wwdg_window;

HAL_StatusTypeDef HAL_IWDG_Init(IWDG_HandleTypeDef *iwdg_h) {
  assert(iwdg_h != NULL);
  s_iwdg_prescaler = iwdg_h->Init.Prescaler;
  s_iwdg_reload = iwdg_h->Init.Reload;
  return HAL_OK;
}

HAL_StatusTypeDef HAL_WWDG_Init(WWDG_HandleTypeDef *wwdg_h) {
  assert(wwdg_h != NULL);
  s_wwdg_prescaler = wwdg_h->Init.Prescaler;
  s_wwdg_window = wwdg_h->Init.Window;
  return HAL_OK;
}

HAL_StatusTypeDef HAL_IWDG_Refresh(IWDG_HandleTypeDef *iwdg_h) {
  assert(iwdg_h != NULL);
  return HAL_OK;
}

HAL_StatusTypeDef HAL_WWDG_Refresh(WWDG_HandleTypeDef *wwdg_h) {
  assert(wwdg_h != NULL);
  return HAL_OK;
}

uint32_t HAL_RCC_GetPCLK1Freq(void) {
  return 36000000u;
}

int main(void) {
  assert(
    platform_iwdg_backend_start(4u, 999u) == PLATFORM_STATUS_OK
  );
  assert(s_iwdg_prescaler == IWDG_PRESCALER_4);
  assert(s_iwdg_prescaler == 0u);
  assert(s_iwdg_reload == 999u);

  assert(
    platform_wwdg_backend_start(1u, 0x50u) == PLATFORM_STATUS_OK
  );
  assert(s_wwdg_prescaler == WWDG_PRESCALER_1);
  assert(s_wwdg_prescaler == 0u);
  assert(s_wwdg_window == 0x50u);
  return 0;
}
