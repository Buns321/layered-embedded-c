//
// Created by Buns_ on 2026/8/24.
//

#include "../Inc/motor_tb6612.h"
#include "../../System/Inc/system_math.h"
#include <assert.h>

/* ========= TB6612 Truth Table =========
 * |------|------|-----|-----|----------|
 * | STBY | PWMA | IN1 | IN2 | Function |
 * |------|------|-----|-----|----------|
 * | H    | H    | H   | L   | Forward  |
 * | H    | H    | L   | H   | Reverse  |
 * | H    | H    | H   | H   | Brake    |
 * | H    | H    | L   | L   | Coast    |
 * | X    | L    | X   | X   | Coast    |
 * | L    | X    | X   | X   | STBY     |
 * |------|------|-----|-----|----------|
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

static void Dev_Motor_TB6612_SetPower(motor_base_t *this, float power) {
  assert(this);
  motor_tb6612_t *self = (motor_tb6612_t *)this;

  float max_duty = (float)self->motor_base.max_duty;
  if (!SYS_ISFINITE(power)) power = 0.0f;
  power = SYS_CLAMP(power, -max_duty, max_duty);

  int8_t next_direction;

  if (power > 0.0f) next_direction = 1;
  else if (power < 0.0f) next_direction = -1;
  else next_direction = 0;


  if (next_direction != self->last_direction) {
    Platform_PWM_SetDuty(
      self->pwm_instance,
      self->pwm_channel,
      self->motor_base.max_duty,
      0
    );

    Platform_GPIO_Pin_State ain1, ain2;

    if      (next_direction > 0) { ain1 = PLATFORM_GPIO_PIN_SET;   ain2 = PLATFORM_GPIO_PIN_RESET; }
    else if (next_direction < 0) { ain1 = PLATFORM_GPIO_PIN_RESET; ain2 = PLATFORM_GPIO_PIN_SET;   }
    else                         { ain1 = PLATFORM_GPIO_PIN_RESET; ain2 = PLATFORM_GPIO_PIN_RESET; }

    Platform_GPIO_WritePin(self->ain1_port, self->ain1_pin, ain1);
    Platform_GPIO_WritePin(self->ain2_port, self->ain2_pin, ain2);

    self->last_direction = next_direction;
  }
  uint32_t duty = (uint32_t)(power < 0.0f ? -power : power);

  Platform_PWM_SetDuty(
    self->pwm_instance,
    self->pwm_channel,
    self->motor_base.max_duty,
    duty
  );
}

static void Dev_Motor_TB6612_Brake(motor_base_t *this) {
  assert(this);

  motor_tb6612_t *self = (motor_tb6612_t *)this;

  Platform_PWM_SetDuty(
    self->pwm_instance,
    self->pwm_channel,
    self->motor_base.max_duty,
    0
  );

  Platform_GPIO_WritePin(
    self->ain1_port,
    self->ain1_pin,
    PLATFORM_GPIO_PIN_SET
  );
  Platform_GPIO_WritePin(
    self->ain2_port,
    self->ain2_pin,
    PLATFORM_GPIO_PIN_SET
  );

  self->last_direction = DEV_TB6612_DIRECTION_BRAKE;

  Platform_PWM_SetDuty(
    self->pwm_instance,
    self->pwm_channel,
    self->motor_base.max_duty,
    (uint32_t)self->motor_base.max_duty
  );
}

static const motor_ops_t motor_ops_tb6612 = {
  .enable    = Dev_Motor_TB6612_Enable,
  .disable   = Dev_Motor_TB6612_Disable,
  .set_power = Dev_Motor_TB6612_SetPower,
  .brake     = Dev_Motor_TB6612_Brake,
};

Platform_StatusTypeDef Dev_Motor_TB6612_Init(
  motor_tb6612_t        *this,
  const char            *name,
  controller_base_t     *controller,
  encoder_base_t        *encoder,
  float                  counts_per_meter,
  float                  control_period,
  float                  target_speed,
  int32_t                max_duty,
  Platform_TIM_Instance  pwm_instance,
  Platform_TIM_Channel   pwm_channel,
  Platform_GPIO_Port     ain1_port, uint16_t ain1_pin,
  Platform_GPIO_Port     ain2_port, uint16_t ain2_pin,
  Platform_GPIO_Port     stby_port, uint16_t stby_pin
) {

  assert(this);

  Dev_Motor_Base_Init(
    &this->motor_base,
    name,
    &motor_ops_tb6612,
    controller,
    encoder,
    counts_per_meter,
    control_period,
    target_speed,
    max_duty
  );

  this->ain1_port    = ain1_port;     this->ain1_pin    = ain1_pin;
  this->ain2_port    = ain2_port;     this->ain2_pin    = ain2_pin;
  this->stby_port    = stby_port;     this->stby_pin    = stby_pin;
  this->pwm_instance = pwm_instance;  this->pwm_channel = pwm_channel;

  this->last_direction = DEV_TB6612_DIRECTION_COAST;

  Platform_GPIO_WritePin(ain1_port, ain1_pin, PLATFORM_GPIO_PIN_RESET);
  Platform_GPIO_WritePin(ain2_port, ain2_pin, PLATFORM_GPIO_PIN_RESET);
  Platform_GPIO_WritePin(stby_port, stby_pin, PLATFORM_GPIO_PIN_RESET);

  Platform_StatusTypeDef status = Platform_PWM_Start(pwm_instance, pwm_channel);

  if (status != Platform_OK) {
    Platform_GPIO_WritePin(
      stby_port,
      stby_pin,
      PLATFORM_GPIO_PIN_RESET
    );

    this->last_direction = DEV_TB6612_DIRECTION_COAST;
    return status;
  }

  return Platform_OK;
}
