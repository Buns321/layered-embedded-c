/**
 * @file bsp_gpio.c
 * @brief GPIO 描述符读写翻转实现。
 * @ingroup bsp
 */

#include "bsp_gpio.h"
#include <stddef.h>

bool BSP_GPIO_IsValid(bsp_gpio_t gpio) {
  return gpio.port != NULL && gpio.pin != 0U &&
         ((uint16_t)(gpio.pin & (uint16_t)(gpio.pin - 1U)) == 0U);
}

void BSP_GPIO_Write(bsp_gpio_t gpio, bool high) {
  if (!BSP_GPIO_IsValid(gpio)) {
    return;
  }
  HAL_GPIO_WritePin(gpio.port, gpio.pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool BSP_GPIO_Read(bsp_gpio_t gpio) {
  if (!BSP_GPIO_IsValid(gpio)) {
    return false;
  }
  return HAL_GPIO_ReadPin(gpio.port, gpio.pin) == GPIO_PIN_SET;
}

void BSP_GPIO_Toggle(bsp_gpio_t gpio) {
  if (!BSP_GPIO_IsValid(gpio)) {
    return;
  }
  HAL_GPIO_TogglePin(gpio.port, gpio.pin);
}
