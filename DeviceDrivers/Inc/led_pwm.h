//
// Created by Buns_ on 2026/10/20.
//

/**
 * @file led_pwm.h
 * @brief PWM LED 具体实现。
 * @addtogroup drivers_concrete
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_LED_PWM_H
#define LAYERED_EMBEDDED_C_LED_PWM_H

#include "led_base.h"
#include "platform_pwm.h"
#include <stdint.h>

/**
 * @brief PWM LED 对象。
 * @note 亮度 0..UINT16_MAX 映射到 0..100% 占空比。
 */
typedef struct led_pwm {
  led_base_t            led_base;
  Platform_TIM_Instance pwm_instance;
  Platform_TIM_Channel  pwm_channel;
} led_pwm_t;

/**
 * @brief 初始化 PWM LED。
 * @param this 对象。
 * @param name 名称。
 * @param pwm_instance 定时器身份。
 * @param pwm_channel 通道身份。
 * @retval Platform_OK PWM 启动成功。
 * @retval Platform_ERROR PWM 启动失败。
 * @note 初始化时先把占空比置 0。
 */
Platform_StatusTypeDef Dev_LED_PWM_Init(
  led_pwm_t            *this,
  const char           *name,
  Platform_TIM_Instance pwm_instance,
  Platform_TIM_Channel  pwm_channel
);

/** @} */

#endif //LAYERED_EMBEDDED_C_LED_PWM_H
