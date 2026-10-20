//
// Created by Buns_ on 2026/8/23.
//

/**
 * @file platform_pwm.h
 * @brief PWM 抽象：启动/停止、比较值与占空比设置。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_PWM_H
#define LAYERED_EMBEDDED_C_PLATFORM_PWM_H

#include "platform_def.h"
#include "platform_tim.h"
#include <stdint.h>

/**
 * @brief 启动指定定时器通道的 PWM 输出。
 * @param tim 定时器身份。
 * @param channel 通道身份。
 * @retval Platform_OK 成功。
 * @retval Platform_ERROR 失败。
 * @retval Platform_BUSY 忙。
 */
Platform_StatusTypeDef Platform_PWM_Start(Platform_TIM_Instance tim, Platform_TIM_Channel channel);

/**
 * @brief 停止指定定时器通道的 PWM 输出。
 * @param tim 定时器身份。
 * @param channel 通道身份。
 * @retval Platform_OK 成功。
 * @retval Platform_ERROR 失败。
 * @retval Platform_BUSY 忙。
 */
Platform_StatusTypeDef Platform_PWM_Stop(Platform_TIM_Instance tim, Platform_TIM_Channel channel);

/**
 * @brief 设置比较值。
 * @param tim 定时器身份。
 * @param channel 通道身份。
 * @param compare 比较值，内部会夹紧到 ARR。
 */
void Platform_PWM_SetCompare(Platform_TIM_Instance tim, Platform_TIM_Channel channel, uint32_t compare);

/**
 * @brief 按占空比设置比较值。
 * @param tim 定时器身份。
 * @param channel 通道身份。
 * @param max_duty 占空比量程，必须大于 0。
 * @param duty 目标占空比，内部会夹紧到 max_duty。
 * @note 实际 CCR = ARR * duty / max_duty，使用 64 位中间量避免溢出。
 */
void Platform_PWM_SetDuty(Platform_TIM_Instance tim, Platform_TIM_Channel channel, uint32_t max_duty, uint32_t duty);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_PWM_H
