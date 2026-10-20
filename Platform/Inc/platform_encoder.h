//
// Created by Buns_ on 2026/8/24.
//

/**
 * @file platform_encoder.h
 * @brief 编码器定时器抽象：启动、计数器读写与位宽查询。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_ENCODER_H
#define LAYERED_EMBEDDED_C_PLATFORM_ENCODER_H

#include "platform_def.h"
#include "platform_tim.h"
#include <stdint.h>

/**
 * @brief 启动编码器接口。
 * @param tim 定时器身份。
 * @param channel 通道身份（通常为 PLATFORM_TIM_CHANNEL_ALL）。
 * @retval Platform_OK 成功。
 * @retval Platform_ERROR 失败。
 */
Platform_StatusTypeDef Platform_Encoder_Start(Platform_TIM_Instance tim, Platform_TIM_Channel channel);

/**
 * @brief 读取硬件计数器。
 * @param tim 定时器身份。
 * @return 当前计数值。
 */
uint32_t Platform_Encoder_GetCounter(Platform_TIM_Instance tim);

/**
 * @brief 写入硬件计数器。
 * @param tim 定时器身份。
 * @param counter 目标计数值。
 */
void Platform_Encoder_SetCounter(Platform_TIM_Instance tim, uint32_t counter);

/**
 * @brief 查询定时器计数位宽。
 * @param tim 定时器身份。
 * @return 16 或 32。
 */
uint8_t Platform_Encoder_GetCounterBits(Platform_TIM_Instance tim);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_ENCODER_H
