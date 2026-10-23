//
// 由 Buns_ 创建于 2026/8/15。
//

/**
 * @file platform_gpio.h
 * @brief GPIO 抽象：端口/引脚枚举与读写接口。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_GPIO_H
#define LAYERED_EMBEDDED_C_PLATFORM_GPIO_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

/**
 * @brief 逻辑端口身份。
 */
typedef enum platform_gpio_port_e {
  PLATFORM_GPIO_PORT_A = 0u,
  PLATFORM_GPIO_PORT_B,
  PLATFORM_GPIO_PORT_C,
#if PLATFORM_ENABLE_GPIO_D
  PLATFORM_GPIO_PORT_D = 3u,
#endif
#if PLATFORM_ENABLE_GPIO_E
  PLATFORM_GPIO_PORT_E = 4u,
#endif
} platform_gpio_port_t;

/**
 * @brief 逻辑引脚电平。
 * @note 这是逻辑状态，实际高低电平由具体驱动/板级决定。
 */
typedef enum platform_gpio_pin_state_e {
  PLATFORM_GPIO_PIN_SET = 0u,
  PLATFORM_GPIO_PIN_RESET,
} platform_gpio_pin_state_t;

/**
 * @brief 读取单个引脚的电平。
 * @param GPIOx 端口身份。
 * @param GPIO_Pin 单引脚掩码。
 * @retval PLATFORM_GPIO_PIN_SET 高电平。
 * @retval PLATFORM_GPIO_PIN_RESET 低电平。
 * @note 传入多 bit 掩码时行为由底层 HAL 决定。
 */
platform_gpio_pin_state_t Platform_GPIO_ReadPin (platform_gpio_port_t GPIOx, uint16_t GPIO_Pin);

/**
 * @brief 写单个引脚。
 * @param GPIOx 端口身份。
 * @param GPIO_Pin 单引脚掩码。
 * @param PinState 目标逻辑电平。
 */
void Platform_GPIO_WritePin(platform_gpio_port_t GPIOx, uint16_t GPIO_Pin, platform_gpio_pin_state_t PinState);

/**
 * @brief 翻转单个引脚。
 * @param GPIOx 端口身份。
 * @param GPIO_Pin 单引脚掩码。
 */
void Platform_GPIO_TogglePin(platform_gpio_port_t GPIOx, uint16_t GPIO_Pin);

/**
 * @brief 锁定引脚配置。
 * @param GPIOx 端口身份。
 * @param GPIO_Pin 单引脚掩码。
 * @retval PLATFORM_STATUS_OK 成功。
 * @retval PLATFORM_STATUS_ERROR 失败。
 */
platform_status_t Platform_GPIO_LockPin(platform_gpio_port_t GPIOx, uint16_t GPIO_Pin);

/** @brief 读取最近一次 GPIO 操作错误快照。 */
uint32_t Platform_GPIO_GetLastError(platform_gpio_port_t GPIOx);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_GPIO_H
