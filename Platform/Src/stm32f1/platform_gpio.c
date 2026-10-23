/**
 * @file platform_gpio.c
 * @brief STM32F1 HAL backend for the common GPIO implementation.
 */

#include "platform_gpio_backend.h"
#include "platform_hal_internal.h"
#include "stm32f1xx_hal.h"

static GPIO_TypeDef *platform_gpio_to_hal_port(platform_gpio_port_t port) {
  switch (port) {
    case PLATFORM_GPIO_PORT_A: return GPIOA;
    case PLATFORM_GPIO_PORT_B: return GPIOB;
    case PLATFORM_GPIO_PORT_C: return GPIOC;
#if PLATFORM_ENABLE_GPIO_D
    case PLATFORM_GPIO_PORT_D: return GPIOD;
#endif
#if PLATFORM_ENABLE_GPIO_E
    case PLATFORM_GPIO_PORT_E: return GPIOE;
#endif
    default: return NULL;
  }
}

static GPIO_PinState platform_gpio_to_hal_state(
  platform_gpio_pin_state_t state
) {
  return state == PLATFORM_GPIO_PIN_SET ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

static platform_gpio_pin_state_t platform_gpio_from_hal_state(
  GPIO_PinState state
) {
  return state == GPIO_PIN_SET
           ? PLATFORM_GPIO_PIN_SET
           : PLATFORM_GPIO_PIN_RESET;
}

bool platform_gpio_backend_is_valid_port(platform_gpio_port_t port) {
  return platform_gpio_to_hal_port(port) != NULL;
}

platform_gpio_pin_state_t platform_gpio_backend_read(
  platform_gpio_port_t port,
  uint16_t pin
) {
  return platform_gpio_from_hal_state(
    HAL_GPIO_ReadPin(platform_gpio_to_hal_port(port), pin)
  );
}

void platform_gpio_backend_write(
  platform_gpio_port_t port,
  uint16_t pin,
  platform_gpio_pin_state_t state
) {
  HAL_GPIO_WritePin(
    platform_gpio_to_hal_port(port),
    pin,
    platform_gpio_to_hal_state(state)
  );
}

void platform_gpio_backend_toggle(
  platform_gpio_port_t port,
  uint16_t pin
) {
  HAL_GPIO_TogglePin(platform_gpio_to_hal_port(port), pin);
}

platform_status_t platform_gpio_backend_lock(
  platform_gpio_port_t port,
  uint16_t pin
) {
  return platform_from_hal_status(
    HAL_GPIO_LockPin(platform_gpio_to_hal_port(port), pin)
  );
}
