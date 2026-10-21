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

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

/**
 * @brief 可用的 PWM 定时器身份。
 * @note 未启用的实例不会进入该枚举。
 */
typedef enum {
#if PLATFORM_ENABLE_PWM_TIM1
  PLATFORM_PWM_TIM1 = 0,
#endif
#if PLATFORM_ENABLE_PWM_TIM3
  PLATFORM_PWM_TIM3 = 3,
#endif
} Platform_PWM_Instance;

/**
 * @brief PWM 通道身份。
 */
typedef enum {
  PLATFORM_PWM_CHANNEL_1 = 0U,
  PLATFORM_PWM_CHANNEL_2,
  PLATFORM_PWM_CHANNEL_3,
  PLATFORM_PWM_CHANNEL_4,
} Platform_PWM_Channel;

#if !PLATFORM_ENABLE_PWM_TIM1 && !PLATFORM_ENABLE_PWM_TIM3
#error "At least one Platform PWM instance must be enabled"
#endif

/**
 * @brief 启动指定 PWM 定时器的单通道输出。
 * @param pwm PWM 定时器身份。
 * @param channel 通道身份。
 * @retval Platform_OK 成功。
 * @retval Platform_ERROR 失败。
 * @retval Platform_BUSY 忙。
 * @pre 对应定时器的 MX_TIMx_Init() 已完成并配置为 PWM 模式。
 */
Platform_StatusTypeDef Platform_PWM_Start(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel
);

/**
 * @brief 启动指定 PWM 定时器的全部四个通道。
 * @param pwm PWM 定时器身份。
 * @retval Platform_OK 成功。
 * @retval Platform_ERROR 失败。
 * @retval Platform_BUSY 忙。
 * @pre 对应定时器的 MX_TIMx_Init() 已完成并配置为 PWM 模式。
 */
Platform_StatusTypeDef Platform_PWM_StartAll(Platform_PWM_Instance pwm);

/**
 * @brief 停止指定 PWM 定时器的单通道输出。
 * @param pwm PWM 定时器身份。
 * @param channel 通道身份。
 * @retval Platform_OK 成功。
 * @retval Platform_ERROR 失败。
 * @retval Platform_BUSY 忙。
 */
Platform_StatusTypeDef Platform_PWM_Stop(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel
);

/**
 * @brief 停止指定 PWM 定时器的全部四个通道。
 * @param pwm PWM 定时器身份。
 * @retval Platform_OK 成功。
 * @retval Platform_ERROR 失败。
 * @retval Platform_BUSY 忙。
 */
Platform_StatusTypeDef Platform_PWM_StopAll(Platform_PWM_Instance pwm);

/**
 * @brief 设置比较值。
 * @param pwm PWM 定时器身份。
 * @param channel 通道身份。
 * @param compare 比较值，内部会夹紧到 ARR。
 */
void Platform_PWM_SetCompare(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel,
  uint32_t compare
);

/**
 * @brief 按占空比设置比较值。
 * @param pwm PWM 定时器身份。
 * @param channel 通道身份。
 * @param max_duty 占空比量程，必须大于 0。
 * @param duty 目标占空比，内部会夹紧到 max_duty。
 * @note 实际 CCR = ARR * duty / max_duty，使用 64 位中间量避免溢出。
 */
void Platform_PWM_SetDuty(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel,
  uint32_t max_duty,
  uint32_t duty
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_PWM_H
