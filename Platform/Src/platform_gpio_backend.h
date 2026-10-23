/**
 * @file platform_gpio_backend.h
 * @brief Private STM32-family backend for the common GPIO implementation.
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_GPIO_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_GPIO_BACKEND_H

#include "platform_gpio.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

bool platform_gpio_backend_is_valid_port(Platform_GPIO_Port port);

Platform_GPIO_Pin_State platform_gpio_backend_read(
  Platform_GPIO_Port port,
  uint16_t pin
);

void platform_gpio_backend_write(
  Platform_GPIO_Port port,
  uint16_t pin,
  Platform_GPIO_Pin_State state
);

void platform_gpio_backend_toggle(
  Platform_GPIO_Port port,
  uint16_t pin
);

Platform_StatusTypeDef platform_gpio_backend_lock(
  Platform_GPIO_Port port,
  uint16_t pin
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_GPIO_BACKEND_H
