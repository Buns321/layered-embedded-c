/**
 * @file platform_gpio_backend.h
 * @brief STM32 系列公共 GPIO 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_GPIO_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_GPIO_BACKEND_H

#include "platform_gpio.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

bool platform_gpio_backend_is_valid_port(platform_gpio_port_t port);

platform_gpio_pin_state_t platform_gpio_backend_read(
  platform_gpio_port_t port,
  uint16_t pin
);

void platform_gpio_backend_write(
  platform_gpio_port_t port,
  uint16_t pin,
  platform_gpio_pin_state_t state
);

void platform_gpio_backend_toggle(
  platform_gpio_port_t port,
  uint16_t pin
);

platform_status_t platform_gpio_backend_lock(
  platform_gpio_port_t port,
  uint16_t pin
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_GPIO_BACKEND_H
