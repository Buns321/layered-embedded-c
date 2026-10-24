/**
 * @file platform_gpio_common.c
 * @brief 与芯片系列无关的 GPIO 参数校验与错误快照。
 * @ingroup platform
 */

#include "platform_error_internal.h"
#include "platform_gpio_backend.h"

static volatile uint32_t gpio_error[5];

static platform_status_t platform_gpio_invalid(
  platform_gpio_port_t port
) {
  if ((uint32_t)port < 5U) {
    platform_error_record(
      &gpio_error[port],
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
  }
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

static bool platform_gpio_state_valid(platform_gpio_pin_state_t state) {
  return state == PLATFORM_GPIO_PIN_SET ||
         state == PLATFORM_GPIO_PIN_RESET;
}

platform_gpio_pin_state_t Platform_GPIO_ReadPin(
  platform_gpio_port_t GPIOx,
  uint16_t GPIO_Pin
) {
  if ((uint32_t)GPIOx < 5U) {
    platform_error_reset(&gpio_error[GPIOx]);
  }
  if (
    !platform_gpio_backend_is_valid_port(GPIOx) ||
    GPIO_Pin == 0U
  ) {
    (void)platform_gpio_invalid(GPIOx);
    return PLATFORM_GPIO_PIN_RESET;
  }
  return platform_gpio_backend_read(GPIOx, GPIO_Pin);
}

void Platform_GPIO_WritePin(
  platform_gpio_port_t GPIOx,
  uint16_t GPIO_Pin,
  platform_gpio_pin_state_t PinState
) {
  if ((uint32_t)GPIOx < 5U) {
    platform_error_reset(&gpio_error[GPIOx]);
  }
  if (
    !platform_gpio_backend_is_valid_port(GPIOx) ||
    GPIO_Pin == 0U ||
    !platform_gpio_state_valid(PinState)
  ) {
    (void)platform_gpio_invalid(GPIOx);
    return;
  }
  platform_gpio_backend_write(GPIOx, GPIO_Pin, PinState);
}

void Platform_GPIO_TogglePin(
  platform_gpio_port_t GPIOx,
  uint16_t GPIO_Pin
) {
  if ((uint32_t)GPIOx < 5U) {
    platform_error_reset(&gpio_error[GPIOx]);
  }
  if (
    !platform_gpio_backend_is_valid_port(GPIOx) ||
    GPIO_Pin == 0U
  ) {
    (void)platform_gpio_invalid(GPIOx);
    return;
  }
  platform_gpio_backend_toggle(GPIOx, GPIO_Pin);
}

platform_status_t Platform_GPIO_LockPin(
  platform_gpio_port_t GPIOx,
  uint16_t GPIO_Pin
) {
  if ((uint32_t)GPIOx < 5U) {
    platform_error_reset(&gpio_error[GPIOx]);
  }
  if (
    !platform_gpio_backend_is_valid_port(GPIOx) ||
    GPIO_Pin == 0U
  ) {
    return platform_gpio_invalid(GPIOx);
  }

  platform_status_t status =
    platform_gpio_backend_lock(GPIOx, GPIO_Pin);
  if (status != PLATFORM_STATUS_OK) {
    platform_error_record(&gpio_error[GPIOx], platform_error_from_status(status));
  }
  return status;
}

uint32_t Platform_GPIO_GetLastError(platform_gpio_port_t GPIOx) {
  return (uint32_t)GPIOx < 5U
           ? gpio_error[GPIOx]
           : PLATFORM_ERROR_INVALID_ARGUMENT;
}
