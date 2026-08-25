//
// Created by Buns_ on 2026/8/24.
//

#ifndef LAYERED_EMBEDDED_C_MOTOR_TB6612_H
#define LAYERED_EMBEDDED_C_MOTOR_TB6612_H

#include "../Base/Inc/motor_base.h"
#include "../../Platform/Inc/platform_pwm.h"
#include "../../Platform/Inc/platform_gpio.h"

typedef struct {
  motor_base_t          motor_base;
  Platform_TIM_Instance pwm_instance;  Platform_TIM_Channel pwm_channel;
  Platform_GPIO_Port    ain1_port;     uint16_t             ain1_pin;
  Platform_GPIO_Port    ain2_port;     uint16_t             ain2_pin;
  Platform_GPIO_Port    stby_port;     uint16_t             stby_pin;
} motor_tb6612_t;

void Dev_Motor_TB6612_Init(
  motor_tb6612_t        *this,
  const char            *name,         int32_t              max_speed,
  Platform_TIM_Instance pwm_instance,  Platform_TIM_Channel pwm_channel,
  Platform_GPIO_Port    ain1_port,     uint16_t             ain1_pin,
  Platform_GPIO_Port    ain2_port,     uint16_t             ain2_pin,
  Platform_GPIO_Port    stby_port,     uint16_t             stby_pin);

#endif //LAYERED_EMBEDDED_C_MOTOR_TB6612_H
