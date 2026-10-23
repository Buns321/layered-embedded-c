/**
 * @file platform_exti.c
 * @brief STM32F4 HAL entry for the common EXTI dispatcher.
 */

#include "platform_exti_backend.h"
#include "stm32f4xx_hal.h"

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  platform_exti_dispatch_event(GPIO_Pin);
}
