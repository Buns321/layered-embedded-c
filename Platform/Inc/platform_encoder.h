//
// Created by Buns_ on 2026/8/24.
//

#ifndef LAYERED_EMBEDDED_C_PLATFORM_ENCODER_H
#define LAYERED_EMBEDDED_C_PLATFORM_ENCODER_H

#include "platform_def.h"
#include "platform_tim.h"
#include <stdint.h>

Platform_StatusTypeDef Platform_Encoder_Start(Platform_TIM_Instance tim, Platform_TIM_Channel channel);
uint32_t Platform_Encoder_GetCounter(Platform_TIM_Instance tim);
void Platform_Encoder_SetCounter(Platform_TIM_Instance tim, uint32_t counter);

#endif //LAYERED_EMBEDDED_C_PLATFORM_ENCODER_H
