//
// 由 Buns_ 创建于 2026/8/23。
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
 * @note 通道 1..4 对应硬件比较/捕获通道；ALL 只可用于同时启动或停止全部通道，
 *       不能用于需要单一通道的比较值写入。
 */
typedef enum platform_tim_channel_e {
  PLATFORM_TIM_CHANNEL_1 = 0U,
  PLATFORM_TIM_CHANNEL_2,
  PLATFORM_TIM_CHANNEL_3,
  PLATFORM_TIM_CHANNEL_4,
  PLATFORM_TIM_CHANNEL_ALL,
} platform_tim_channel_t;

/**
 * @brief 定时器实例身份。
 * @note 枚举值是身份编码而非连续数组下标；F1 只提供 TIM1..TIM4，F4 扩展到 TIM14。
 * @note 本类型只表达硬件身份，PWM 与 Encoder 的可用集合仍由各自功能实例定义。
 */
/* STM32F1 构建中省略 TIM5-TIM14。 */
typedef enum platform_tim_instance_e {
  PLATFORM_TIM_TIM1 = 0U,
  PLATFORM_TIM_TIM2,
  PLATFORM_TIM_TIM3,
  PLATFORM_TIM_TIM4,
#if defined(STM32F4xx)
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
#endif
} platform_tim_instance_t;

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_TIM_H
