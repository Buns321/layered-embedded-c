//
// Created by Buns_ on 2026/8/15.
//

#ifndef LAYERED_EMBEDDED_C_LED_GPIO_H
#define LAYERED_EMBEDDED_C_LED_GPIO_H

#include "../Base/Inc/led_base.h"
#include "../../Platform/Inc/platform_gpio.h"

typedef struct led_gpio {
  led_base_t              led_base;
  Platform_GPIO_Port      port;
  uint16_t                pin;
  Platform_GPIO_Pin_State active_level;
} led_gpio_t;

void Dev_LED_GPIO_Init(led_gpio_t *this, const char *name, Platform_GPIO_Port port, uint16_t pin, Platform_GPIO_Pin_State active_level);

#endif //LAYERED_EMBEDDED_C_LED_GPIO_H
