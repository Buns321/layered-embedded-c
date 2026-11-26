/**
 * @file bsp_gpio.c
 * @brief GPIO 描述符读写翻转实现。
 * @details 对 `bsp_gpio_t` 进行单比特掩码有效性校验后转发至 STM32 HAL GPIO 原语，
 *          对未连接的占位引脚（port == NULL）执行安全空操作。
 * @ingroup BSP
 */

#include "bsp_gpio.h"
#include <stddef.h>

bool BSP_GPIO_IsValid(bsp_gpio_t gpio) {
  // 利用 (pin & (pin - 1)) == 0 判定 pin 仅为单个置位比特，防止多引脚掩码误操作
  return gpio.port != NULL && gpio.pin != 0U &&
         ((uint16_t)(gpio.pin & (uint16_t)(gpio.pin - 1U)) == 0U);
}

void BSP_GPIO_Write(bsp_gpio_t gpio, bool high) {
  if (!BSP_GPIO_IsValid(gpio)) {
    return;
  }
  // HAL_GPIO_WritePin 内部写 BSRR 寄存器，属于硬件单指令原子操作，无需关中断
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
