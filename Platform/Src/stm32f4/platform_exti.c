/**
 * @file platform_exti.c
 * @brief STM32F4 公共 EXTI 分发器的 HAL 入口。
 * @ingroup platform
 */

#include "platform_exti_backend.h"
#include "stm32f4xx_hal.h"

/**
 * @brief 把 GPIO EXTI 中断转发给已注册回调。
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  platform_exti_dispatch_event(GPIO_Pin);
}
