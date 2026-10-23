/**
 * @file platform_gpio_common.c
 * @brief Family-independent GPIO validation and error snapshot.
 */

#include "platform_error_internal.h"
#include "platform_gpio_backend.h"

static volatile uint32_t gpio_error[5];

static Platform_StatusTypeDef platform_gpio_invalid(
  Platform_GPIO_Port port
) {
  if ((uint32_t)port < 5U) {
    platform_error_record(
      &gpio_error[port],
      PLATFORM_COMMON_ERROR_PARAMETER
    );
  }
  return Platform_INVALID_ARGUMENT;
}

static bool platform_gpio_state_valid(Platform_GPIO_Pin_State state) {
  return state == PLATFORM_GPIO_PIN_SET ||
         state == PLATFORM_GPIO_PIN_RESET;
}

Platform_GPIO_Pin_State Platform_GPIO_ReadPin(
  Platform_GPIO_Port GPIOx,
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
  Platform_GPIO_Port GPIOx,
  uint16_t GPIO_Pin,
  Platform_GPIO_Pin_State PinState
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
  Platform_GPIO_Port GPIOx,
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

Platform_StatusTypeDef Platform_GPIO_LockPin(
  Platform_GPIO_Port GPIOx,
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

  Platform_StatusTypeDef status =
    platform_gpio_backend_lock(GPIOx, GPIO_Pin);
  if (status != Platform_OK) {
    platform_error_record(&gpio_error[GPIOx], platform_error_from_status(status));
  }
  return status;
}

uint32_t Platform_GPIO_GetLastError(Platform_GPIO_Port GPIOx) {
  return (uint32_t)GPIOx < 5U
           ? gpio_error[GPIOx]
           : PLATFORM_COMMON_ERROR_PARAMETER;
}
