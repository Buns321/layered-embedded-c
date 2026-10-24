//
// 由 Buns_ 创建于 2026/8/24。
//

/**
 * @file motor_tb6612.c
 * @brief TB6612 电机驱动实现。
 * @ingroup drivers_concrete
 */

#include "motor_tb6612.h"
#include "system_math.h"
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

/*
 * 契约：
 * - power 为 [-motor_base.max_duty, +motor_base.max_duty] 的占空比，符号表示方向；
 * - 方向切换前先把 PWM 撤到 0，再更新 IN1/IN2；
 * - STBY 在板级按组共享（m0/m1、m2/m3），单电机停机不能依赖 STBY；
 * - Init 返回 platform_status_t，PWM 启动失败时保持 STBY 为低。
 */

static void Dev_Motor_TB6612_Enable(motor_base_t *motor_h) {
  assert(motor_h);
  motor_tb6612_t *motor_tb6612_h = (motor_tb6612_t *)motor_h;
  Platform_GPIO_WritePin(motor_tb6612_h->stby_port, motor_tb6612_h->stby_pin, PLATFORM_GPIO_PIN_SET);
}

static void Dev_Motor_TB6612_Disable(motor_base_t *motor_h) {
  assert(motor_h);
  motor_tb6612_t *motor_tb6612_h = (motor_tb6612_t *)motor_h;
  Platform_GPIO_WritePin(motor_tb6612_h->stby_port, motor_tb6612_h->stby_pin, PLATFORM_GPIO_PIN_RESET);
}

static void Dev_Motor_TB6612_SetPower(motor_base_t *motor_h, float power) {
  assert(motor_h);
  motor_tb6612_t *motor_tb6612_h = (motor_tb6612_t *)motor_h;

  float max_duty = (float)motor_tb6612_h->motor_base.max_duty;
  if (!SYS_ISFINITE(power)) power = 0.0f;
  power = SYS_CLAMP(power, -max_duty, max_duty);

  int8_t next_direction;

  if (power > 0.0f) next_direction = 1;
  else if (power < 0.0f) next_direction = -1;
  else next_direction = 0;


  if (next_direction != motor_tb6612_h->last_direction) {
    (void)Platform_PWM_SetDuty(
      motor_tb6612_h->pwm_instance,
      motor_tb6612_h->pwm_channel,
      motor_tb6612_h->motor_base.max_duty,
      0
    );

    platform_gpio_pin_state_t ain1, ain2;

    if      (next_direction > 0) { ain1 = PLATFORM_GPIO_PIN_SET;   ain2 = PLATFORM_GPIO_PIN_RESET; }
    else if (next_direction < 0) { ain1 = PLATFORM_GPIO_PIN_RESET; ain2 = PLATFORM_GPIO_PIN_SET;   }
    else                         { ain1 = PLATFORM_GPIO_PIN_RESET; ain2 = PLATFORM_GPIO_PIN_RESET; }

    Platform_GPIO_WritePin(motor_tb6612_h->ain1_port, motor_tb6612_h->ain1_pin, ain1);
    Platform_GPIO_WritePin(motor_tb6612_h->ain2_port, motor_tb6612_h->ain2_pin, ain2);

    motor_tb6612_h->last_direction = next_direction;
  }
  uint32_t duty = (uint32_t)(power < 0.0f ? -power : power);

  (void)Platform_PWM_SetDuty(
    motor_tb6612_h->pwm_instance,
    motor_tb6612_h->pwm_channel,
    motor_tb6612_h->motor_base.max_duty,
    duty
  );
}

static void Dev_Motor_TB6612_Brake(motor_base_t *motor_h) {
  assert(motor_h);

  motor_tb6612_t *motor_tb6612_h = (motor_tb6612_t *)motor_h;

  (void)Platform_PWM_SetDuty(
    motor_tb6612_h->pwm_instance,
    motor_tb6612_h->pwm_channel,
    motor_tb6612_h->motor_base.max_duty,
    0
  );

  Platform_GPIO_WritePin(
    motor_tb6612_h->ain1_port,
    motor_tb6612_h->ain1_pin,
    PLATFORM_GPIO_PIN_SET
  );
  Platform_GPIO_WritePin(
    motor_tb6612_h->ain2_port,
    motor_tb6612_h->ain2_pin,
    PLATFORM_GPIO_PIN_SET
  );

  motor_tb6612_h->last_direction = DEV_TB6612_DIRECTION_BRAKE;

  (void)Platform_PWM_SetDuty(
    motor_tb6612_h->pwm_instance,
    motor_tb6612_h->pwm_channel,
    motor_tb6612_h->motor_base.max_duty,
    (uint32_t)motor_tb6612_h->motor_base.max_duty
  );
}

static const motor_ops_t motor_ops_tb6612 = {
  .enable    = Dev_Motor_TB6612_Enable,
  .disable   = Dev_Motor_TB6612_Disable,
  .set_power = Dev_Motor_TB6612_SetPower,
  .brake     = Dev_Motor_TB6612_Brake,
};

platform_status_t Dev_Motor_TB6612_Init(
  motor_tb6612_t        *motor_h,
  const char            *name,
  controller_base_t     *controller_h,
  encoder_base_t        *encoder_h,
  float                  counts_per_meter,
  float                  control_period,
  float                  target_speed,
  int32_t                max_duty,
  platform_pwm_instance_t  pwm_instance,
  platform_pwm_channel_t   pwm_channel,
  platform_gpio_port_t     ain1_port, uint16_t ain1_pin,
  platform_gpio_port_t     ain2_port, uint16_t ain2_pin,
  platform_gpio_port_t     stby_port, uint16_t stby_pin
) {

  assert(motor_h);

  Dev_Motor_Base_Init(
    &motor_h->motor_base,
    name,
    &motor_ops_tb6612,
    controller_h,
    encoder_h,
    counts_per_meter,
    control_period,
    target_speed,
    max_duty
  );

  motor_h->ain1_port    = ain1_port;     motor_h->ain1_pin    = ain1_pin;
  motor_h->ain2_port    = ain2_port;     motor_h->ain2_pin    = ain2_pin;
  motor_h->stby_port    = stby_port;     motor_h->stby_pin    = stby_pin;
  motor_h->pwm_instance = pwm_instance;  motor_h->pwm_channel = pwm_channel;

  motor_h->last_direction = DEV_TB6612_DIRECTION_COAST;

  Platform_GPIO_WritePin(ain1_port, ain1_pin, PLATFORM_GPIO_PIN_RESET);
  Platform_GPIO_WritePin(ain2_port, ain2_pin, PLATFORM_GPIO_PIN_RESET);
  Platform_GPIO_WritePin(stby_port, stby_pin, PLATFORM_GPIO_PIN_RESET);

  platform_status_t status = Platform_PWM_Start(pwm_instance, pwm_channel);

  if (status != PLATFORM_STATUS_OK) {
    Platform_GPIO_WritePin(
      stby_port,
      stby_pin,
      PLATFORM_GPIO_PIN_RESET
    );

    motor_h->last_direction = DEV_TB6612_DIRECTION_COAST;
    return status;
  }

  return PLATFORM_STATUS_OK;
}
