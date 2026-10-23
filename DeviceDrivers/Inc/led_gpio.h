//
// 由 Buns_ 创建于 2026/8/15。
//

/**
 * @file led_gpio.h
 * @brief GPIO LED 具体实现。
 * @addtogroup drivers_concrete
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_LED_GPIO_H
#define LAYERED_EMBEDDED_C_LED_GPIO_H

#include "led_base.h"
#include "platform_gpio.h"

/**
 * @brief GPIO LED 对象。
 * @note active_level 表示“点亮”对应的电平。
 */
typedef struct led_gpio_s {
  led_base_t              led_base;
  platform_gpio_port_t      port;
  uint16_t                pin;
  platform_gpio_pin_state_t active_level;
} led_gpio_t;

/**
 * @brief 初始化 GPIO LED。
 * @param led_h 对象。
 * @param name 名称。
 * @param port 端口。
 * @param pin 引脚掩码。
 * @param active_level 点亮电平。
 * @note 初始化会显式写入 inactive level。
 */
void Dev_LED_GPIO_Init(led_gpio_t *led_h, const char *name, platform_gpio_port_t port, uint16_t pin, platform_gpio_pin_state_t active_level);

/** @} */

#endif //LAYERED_EMBEDDED_C_LED_GPIO_H
