//
// Created by Buns_ on 2026/10/20.
//

#ifndef LAYERED_EMBEDDED_C_LED_PWM_H
#define LAYERED_EMBEDDED_C_LED_PWM_H

#include "led_base.h"
#include "platform_pwm.h"
#include <stdint.h>

typedef struct led_pwm {
  led_base_t            led_base;
  Platform_TIM_Instance pwm_instance;
  Platform_TIM_Channel  pwm_channel;
} led_pwm_t;

Platform_StatusTypeDef Dev_LED_PWM_Init(
  led_pwm_t            *this,
  const char           *name,
  Platform_TIM_Instance pwm_instance,
  Platform_TIM_Channel  pwm_channel
);

#endif //LAYERED_EMBEDDED_C_LED_PWM_H
