//
// Created by Buns_ on 2026/8/15.
//

/**
 * @file platform_gpio.h
 * @brief GPIO 抽象：端口/引脚枚举与读写接口。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_GPIO_H
#define LAYERED_EMBEDDED_C_PLATFORM_GPIO_H

#include "platform_def.h"
#include <stdint.h>

/**
 * @brief 逻辑端口身份。
 */
typedef enum {
  PLATFORM_GPIO_PORT_A = 0u,
  PLATFORM_GPIO_PORT_B,
  PLATFORM_GPIO_PORT_C,
  PLATFORM_GPIO_PORT_D,
  PLATFORM_GPIO_PORT_E,
} Platform_GPIO_Port;

/**
 * @brief 逻辑引脚电平。
 * @note 这是逻辑状态，实际高低电平由具体驱动/板级决定。
 */
typedef enum {
  PLATFORM_GPIO_PIN_SET = 0u,
  PLATFORM_GPIO_PIN_RESET,
} Platform_GPIO_Pin_State;

/**
 * @brief 读取单个引脚的电平。
 * @param GPIOx 端口身份。
 * @param GPIO_Pin 单引脚掩码。
 * @retval PLATFORM_GPIO_PIN_SET 高电平。
 * @retval PLATFORM_GPIO_PIN_RESET 低电平。
 * @note 传入多 bit 掩码时行为由底层 HAL 决定。
 */
Platform_GPIO_Pin_State Platform_GPIO_ReadPin (Platform_GPIO_Port GPIOx, uint16_t GPIO_Pin);

/**
 * @brief 写单个引脚。
 * @param GPIOx 端口身份。
 * @param GPIO_Pin 单引脚掩码。
 * @param PinState 目标逻辑电平。
 */
void Platform_GPIO_WritePin(Platform_GPIO_Port GPIOx, uint16_t GPIO_Pin, Platform_GPIO_Pin_State PinState);

/**
 * @brief 翻转单个引脚。
 * @param GPIOx 端口身份。
 * @param GPIO_Pin 单引脚掩码。
 */
void Platform_GPIO_TogglePin(Platform_GPIO_Port GPIOx, uint16_t GPIO_Pin);

/**
 * @brief 锁定引脚配置。
 * @param GPIOx 端口身份。
 * @param GPIO_Pin 单引脚掩码。
 * @retval Platform_OK 成功。
 * @retval Platform_ERROR 失败。
 */
Platform_StatusTypeDef Platform_GPIO_LockPin(Platform_GPIO_Port GPIOx, uint16_t GPIO_Pin);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_GPIO_H
