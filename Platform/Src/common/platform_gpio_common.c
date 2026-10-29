/**
 * @file platform_gpio_common.c
 * @brief 与芯片系列无关的 GPIO 参数校验与错误快照。
 * @ingroup platform
 */

#include "platform_error_internal.h"
#include "platform_gpio_backend.h"

static volatile uint32_t gpio_error[5];

/**
 * @brief 构造 GPIO 非法参数状态。
 */
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

/**
 * @brief 校验 GPIO 引脚状态枚举。
 */
static bool platform_gpio_state_valid(platform_gpio_pin_state_t state) {
  return state == PLATFORM_GPIO_PIN_SET ||
         state == PLATFORM_GPIO_PIN_RESET;
}

/**
 * @brief 读取单个引脚的电平。
 * @param GPIOx 端口身份。
 * @param GPIO_Pin 单引脚掩码。
 * @retval PLATFORM_GPIO_PIN_SET 高电平。
 * @retval PLATFORM_GPIO_PIN_RESET 低电平。
 * @note 传入多 bit 掩码时行为由底层 HAL 决定。
 */
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

/**
 * @brief 写单个引脚。
 * @param GPIOx 端口身份。
 * @param GPIO_Pin 单引脚掩码。
 * @param PinState 目标逻辑电平。
 */
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

/**
 * @brief 翻转单个引脚。
 * @param GPIOx 端口身份。
 * @param GPIO_Pin 单引脚掩码。
 */
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

/**
 * @brief 锁定引脚配置。
 * @param GPIOx 端口身份。
 * @param GPIO_Pin 单引脚掩码。
 * @retval PLATFORM_STATUS_OK 成功。
 * @retval PLATFORM_STATUS_ERROR 失败。
 */
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

/** @brief 读取最近一次 GPIO 操作错误快照。 */
uint32_t Platform_GPIO_GetLastError(platform_gpio_port_t GPIOx) {
  return (uint32_t)GPIOx < 5U
           ? gpio_error[GPIOx]
           : PLATFORM_ERROR_INVALID_ARGUMENT;
}
