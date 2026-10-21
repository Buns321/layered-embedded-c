//
// Created by Buns_ on 2026/8/23.
//

/**
 * @file platform_tim.h
 * @brief 定时器与通道的身份枚举。
 * @addtogroup platform
 * @{
 * @note 这里只表达“是谁”，具体 PSC/ARR/模式由 CubeMX 或平台实现决定。
 * @note PWM 与 Encoder 使用各自的功能实例类型，不复用本文件的通用身份。
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_TIM_H
#define LAYERED_EMBEDDED_C_PLATFORM_TIM_H

/**
 * @brief 定时器通道身份。
 */
typedef enum {
  PLATFORM_TIM_CHANNEL_1 = 0U,
  PLATFORM_TIM_CHANNEL_2,
  PLATFORM_TIM_CHANNEL_3,
  PLATFORM_TIM_CHANNEL_4,
  PLATFORM_TIM_CHANNEL_ALL,
} Platform_TIM_Channel;

/**
 * @brief 定时器实例身份。
 */
typedef enum {
  PLATFORM_TIM_TIM1 = 0U,
  PLATFORM_TIM_TIM2,
  PLATFORM_TIM_TIM3,
  PLATFORM_TIM_TIM4,
  PLATFORM_TIM_TIM5,
  PLATFORM_TIM_TIM6,
  PLATFORM_TIM_TIM7,
  PLATFORM_TIM_TIM8,
  PLATFORM_TIM_TIM9,
  PLATFORM_TIM_TIM10,
  PLATFORM_TIM_TIM11,
  PLATFORM_TIM_TIM12,
  PLATFORM_TIM_TIM13,
  PLATFORM_TIM_TIM14,
} Platform_TIM_Instance;

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_TIM_H
