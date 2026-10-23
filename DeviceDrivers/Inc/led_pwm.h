//
// 由 Buns_ 创建于 2026/10/20。
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
typedef struct led_pwm_s {
  led_base_t            led_base;
  platform_pwm_instance_t pwm_instance;
  platform_pwm_channel_t  pwm_channel;
} led_pwm_t;

/**
 * @brief 初始化 PWM LED。
 * @param led_h 对象。
 * @param name 名称。
 * @param pwm_instance 定时器身份。
 * @param pwm_channel 通道身份。
 * @retval PLATFORM_STATUS_OK PWM 启动成功。
 * @retval PLATFORM_STATUS_ERROR PWM 启动失败。
 * @note 初始化时先把占空比置 0。
 */
platform_status_t Dev_LED_PWM_Init(
  led_pwm_t             *led_h,
  const char            *name,
  platform_pwm_instance_t  pwm_instance,
  platform_pwm_channel_t   pwm_channel
);

/** @} */

#endif //LAYERED_EMBEDDED_C_LED_PWM_H
