//
// Created by Buns_ on 2026/8/23.
//

#ifndef LAYERED_EMBEDDED_C_PLATFORM_PWM_H
#define LAYERED_EMBEDDED_C_PLATFORM_PWM_H

#include "platform_def.h"
#include "platform_tim.h"
#include <stdint.h>

Platform_StatusTypeDef Platform_PWM_Start(Platform_TIM_Instance tim, Platform_TIM_Channel channel);
Platform_StatusTypeDef Platform_PWM_Stop(Platform_TIM_Instance tim, Platform_TIM_Channel channel);
void Platform_PWM_SetCompare(Platform_TIM_Instance tim, Platform_TIM_Channel channel, uint32_t compare);
void Platform_PWM_SetDuty(Platform_TIM_Instance tim, Platform_TIM_Channel channel, uint32_t max_duty, uint32_t duty);

#endif //LAYERED_EMBEDDED_C_PLATFORM_PWM_H
