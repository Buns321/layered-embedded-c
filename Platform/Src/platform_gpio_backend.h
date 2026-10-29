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

/**
 * @brief 校验 GPIO 端口是否已由板级配置启用。
 */
bool platform_gpio_backend_is_valid_port(platform_gpio_port_t port);

/**
 * @brief 读取GPIO数据。
 */
platform_gpio_pin_state_t platform_gpio_backend_read(
  platform_gpio_port_t port,
  uint16_t pin
);

/**
 * @brief 写入GPIO数据。
 */
void platform_gpio_backend_write(
  platform_gpio_port_t port,
  uint16_t pin,
  platform_gpio_pin_state_t state
);

/**
 * @brief 翻转 GPIO 输出电平。
 */
void platform_gpio_backend_toggle(
  platform_gpio_port_t port,
  uint16_t pin
);

/**
 * @brief 锁定 GPIO 引脚配置，禁止运行期重配置。
 */
platform_status_t platform_gpio_backend_lock(
  platform_gpio_port_t port,
  uint16_t pin
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_GPIO_BACKEND_H
