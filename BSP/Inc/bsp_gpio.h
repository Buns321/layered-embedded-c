/**
 * @file bsp_gpio.h
 * @brief 轻量级 GPIO 引脚描述符与读写翻转接口。
 * @addtogroup bsp
 * @{
 */

#ifndef LAYERED_CAR_BSP_GPIO_H
#define LAYERED_CAR_BSP_GPIO_H

#include "bsp_mcu.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief GPIO 引脚描述符；port == NULL 且 pin == 0 表示可选引脚未连接。
 */
typedef struct bsp_gpio_s {
  GPIO_TypeDef *port;
  uint16_t pin;
} bsp_gpio_t;

/**
 * @brief 校验描述符是否有效（port 非空且 pin 为非零单 bit 掩码）。
 */
bool BSP_GPIO_IsValid(bsp_gpio_t gpio);

/**
 * @brief 写入 GPIO 输出电平；描述符无效时安全忽略（可在任务或 ISR 调用）。
 */
void BSP_GPIO_Write(bsp_gpio_t gpio, bool high);

/**
 * @brief 读取 GPIO 输入电平；描述符无效时返回 false（可在任务或 ISR 调用）。
 */
bool BSP_GPIO_Read(bsp_gpio_t gpio);

/**
 * @brief 翻转 GPIO 输出电平；描述符无效时安全忽略。
 */
void BSP_GPIO_Toggle(bsp_gpio_t gpio);

/** @} */

#endif /* LAYERED_CAR_BSP_GPIO_H */
