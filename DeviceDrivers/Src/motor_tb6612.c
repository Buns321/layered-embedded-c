//
// Created by Buns_ on 2026/8/24.
//

#include "../Inc/motor_tb6612.h"
#include <assert.h>

/*
 * ********* TB6612 Truth Table *********
 * | STBY | PWMA | IN1 | IN2 | Function |
 * | H    | H    | H   | L   | Forward  |
 * | H    | H    | L   | H   | Reverse  |
 * | H    | H    | H   | H   | Brake    |
 * | H    | H    | L   | L   | Coast    |
 * | L    | X    | X   | X   | STBY     |
 */

static void Dev_Motor_TB6612_Enable(motor_base_t *this) {
  assert(this);
  motor_tb6612_t *self = (motor_tb6612_t *)this;
  Platform_GPIO_WritePin(self->stby_port, self->stby_pin, PLATFORM_GPIO_PIN_SET);
}

static void Dev_Motor_TB6612_Disable(motor_base_t *this) {
  assert(this);
  motor_tb6612_t *self = (motor_tb6612_t *)this;
  Platform_GPIO_WritePin(self->stby_port, self->stby_pin, PLATFORM_GPIO_PIN_RESET);
}

static void Dev_Motor_TB6612_SetSpeed(motor_base_t *this, int32_t speed) {
  assert(this);
  motor_tb6612_t *self = (motor_tb6612_t *)this;

  // if (speed > 0) {
  //   Platform_GPIO_WritePin(self->ain1_pin_port, self->ain1_pin, PLATFORM_GPIO_PIN_SET);
  //   Platform_GPIO_WritePin(self->ain2_pin_port, self->ain2_pin, PLATFORM_GPIO_PIN_RESET);
  //   Platform_PWM_SetDuty(self->pwm_instance, self->pwm_channel, max_speed, speed);
  // } else if (speed < 0) {
  //   Platform_GPIO_WritePin(self->ain1_pin_port, self->ain1_pin, PLATFORM_GPIO_PIN_RESET);
  //   Platform_GPIO_WritePin(self->ain2_pin_port, self->ain2_pin, PLATFORM_GPIO_PIN_SET);
  //   Platform_PWM_SetDuty(self->pwm_instance, self->pwm_channel, max_speed, speed);
  // } else if (speed == 0) {
  //   Platform_GPIO_WritePin(self->ain1_pin_port, self->ain1_pin, PLATFORM_GPIO_PIN_RESET);
  //   Platform_GPIO_WritePin(self->ain2_pin_port, self->ain2_pin, PLATFORM_GPIO_PIN_RESET);
  //   Platform_PWM_SetDuty(self->pwm_instance, self->pwm_channel, max_speed, speed);
  // }
  // 这段使用下面的方法写起来更简单

  Platform_GPIO_Pin_State ain1, ain2;
  if      (speed > 0) {ain1 = PLATFORM_GPIO_PIN_SET;   ain2 = PLATFORM_GPIO_PIN_RESET;}
  else if (speed < 0) {ain1 = PLATFORM_GPIO_PIN_RESET; ain2 = PLATFORM_GPIO_PIN_SET;}
  else                   {ain1 = PLATFORM_GPIO_PIN_RESET; ain2 = PLATFORM_GPIO_PIN_RESET;}
  Platform_GPIO_WritePin(self->ain1_port, self->ain1_pin, ain1);
  Platform_GPIO_WritePin(self->ain2_port, self->ain2_pin, ain2);
  Platform_PWM_SetDuty(self->pwm_instance, self->pwm_channel, self->motor_base.max_speed, (uint32_t)(speed < 0 ? -speed : speed));
}

static void Dev_Motor_TB6612_Brake(motor_base_t *this) {
  assert(this);
  motor_tb6612_t *self = (motor_tb6612_t *)this;
  Platform_GPIO_WritePin(self->ain1_port, self->ain1_pin, PLATFORM_GPIO_PIN_SET);
  Platform_GPIO_WritePin(self->ain2_port, self->ain2_pin, PLATFORM_GPIO_PIN_SET);
}

static const motor_ops_t motor_ops_tb6612 = {
  .enable    = Dev_Motor_TB6612_Enable,
  .disable   = Dev_Motor_TB6612_Disable,
  .set_speed = Dev_Motor_TB6612_SetSpeed,
  .brake     = Dev_Motor_TB6612_Brake,
};

void Dev_Motor_TB6612_Init(
  motor_tb6612_t        *this,
  const char            *name,         int32_t              max_speed,
  Platform_TIM_Instance pwm_instance,  Platform_TIM_Channel pwm_channel,
  Platform_GPIO_Port    ain1_port,     uint16_t             ain1_pin,
  Platform_GPIO_Port    ain2_port,     uint16_t             ain2_pin,
  Platform_GPIO_Port    stby_port,     uint16_t             stby_pin) {
  
  assert(this);

  Dev_Motor_Base_Init(&this->motor_base, name, &motor_ops_tb6612, max_speed);

  this->ain1_port    = ain1_port;     this->ain1_pin    = ain1_pin;
  this->ain2_port    = ain2_port;     this->ain2_pin    = ain2_pin;
  this->stby_port    = stby_port;     this->stby_pin    = stby_pin;
  this->pwm_instance = pwm_instance;  this->pwm_channel = pwm_channel;

  Platform_GPIO_WritePin(ain1_port, ain1_pin, PLATFORM_GPIO_PIN_RESET);
  Platform_GPIO_WritePin(ain2_port, ain2_pin, PLATFORM_GPIO_PIN_RESET);
  Platform_GPIO_WritePin(stby_port, stby_pin, PLATFORM_GPIO_PIN_RESET);

  Platform_PWM_Start(pwm_instance, pwm_channel);
}
