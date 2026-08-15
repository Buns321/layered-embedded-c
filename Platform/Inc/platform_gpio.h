//
// Created by Buns_ on 2026/8/15.
//

#ifndef LAYERED_EMBEDDED_C_PLATFORM_GPIO_H
#define LAYERED_EMBEDDED_C_PLATFORM_GPIO_H

#include "platform_def.h"
#include <stdint.h>

typedef enum {
  PLATFORM_GPIO_PORT_A = 0u,
  PLATFORM_GPIO_PORT_B,
  PLATFORM_GPIO_PORT_C,
  PLATFORM_GPIO_PORT_D,
  PLATFORM_GPIO_PORT_E,
  PLATFORM_GPIO_PORT_F,
  PLATFORM_GPIO_PORT_G,
} Platform_GPIO_Port;

typedef enum {
  PLATFORM_GPIO_PIN_SET = 0u,
  PLATFORM_GPIO_PIN_RESET,
} Platform_GPIO_Pin_State;

Platform_GPIO_Pin_State Platform_GPIO_ReadPin (Platform_GPIO_Port GPIOx, uint16_t GPIO_Pin);
void Platform_GPIO_WritePin(Platform_GPIO_Port GPIOx, uint16_t GPIO_Pin, Platform_GPIO_Pin_State PinState);
void Platform_GPIO_TogglePin(Platform_GPIO_Port GPIOx, uint16_t GPIO_Pin);
Platform_StatusTypeDef Platform_GPIO_LockPin(Platform_GPIO_Port GPIOx, uint16_t GPIO_Pin);

#endif //LAYERED_EMBEDDED_C_PLATFORM_GPIO_H
