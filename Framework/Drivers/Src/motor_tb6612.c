/**
 * @file motor_tb6612.c
 * @brief TB6612 电机驱动实现。
 * @ingroup drivers
 */

#include "motor_tb6612.h"
#include "sys_math.h"
#include <stddef.h>

static bool tb6612_pwm_isvalid(const bsp_pwm_t *pwm) {
  if (pwm == NULL || pwm->htim == NULL) {
    return false;
  }
  return pwm->channel == TIM_CHANNEL_1 || pwm->channel == TIM_CHANNEL_2 ||
         pwm->channel == TIM_CHANNEL_3 || pwm->channel == TIM_CHANNEL_4;
}

static void tb6612_enable(motor_base_t *motor_h) {
  motor_tb6612_t *motor = (motor_tb6612_t *)motor_h;
  if (motor != NULL && BSP_GPIO_IsValid(motor->stby)) {
    BSP_GPIO_Write(motor->stby, true);
  }
}

static void tb6612_disable(motor_base_t *motor_h) {
  motor_tb6612_t *motor = (motor_tb6612_t *)motor_h;
  if (motor != NULL && BSP_GPIO_IsValid(motor->stby)) {
    BSP_GPIO_Write(motor->stby, false);
  }
}

static void tb6612_setpower(motor_base_t *motor_h, float power_duty) {
  motor_tb6612_t *motor = (motor_tb6612_t *)motor_h;
  if (motor == NULL) {
    return;
  }

  float max_duty = (float)motor->base.max_duty;
  if (!SYS_ISFINITE(power_duty)) {
    power_duty = 0.0f;
  }
  power_duty = SYS_CLAMP(power_duty, -max_duty, max_duty);

  motor_tb6612_dir_t next_dir = DEV_TB6612_DIR_COAST;
  if (power_duty > 0.0f) {
    next_dir = DEV_TB6612_DIR_FORWARD;
  } else if (power_duty < 0.0f) {
    next_dir = DEV_TB6612_DIR_REVERSE;
  }

  if (next_dir != motor->last_direction) {
    /* 方向切换前必须先撤占空比到 0，避免桥臂直通或电流冲击。 */
    (void)BSP_PWM_SetDuty(&motor->pwm, 0U);
    if (next_dir == DEV_TB6612_DIR_FORWARD) {
      BSP_GPIO_Write(motor->in1, true);
      BSP_GPIO_Write(motor->in2, false);
    } else if (next_dir == DEV_TB6612_DIR_REVERSE) {
      BSP_GPIO_Write(motor->in1, false);
      BSP_GPIO_Write(motor->in2, true);
    } else {
      BSP_GPIO_Write(motor->in1, false);
      BSP_GPIO_Write(motor->in2, false);
    }
    motor->last_direction = next_dir;
  }

  uint32_t duty = (uint32_t)SYS_ABS(power_duty);
  (void)BSP_PWM_SetDuty(&motor->pwm, duty);
}

static void tb6612_brake(motor_base_t *motor_h) {
  motor_tb6612_t *motor = (motor_tb6612_t *)motor_h;
  if (motor == NULL) {
    return;
  }

  (void)BSP_PWM_SetDuty(&motor->pwm, 0U);
  BSP_GPIO_Write(motor->in1, true);
  BSP_GPIO_Write(motor->in2, true);
  motor->last_direction = DEV_TB6612_DIR_BRAKE;
  (void)BSP_PWM_SetDuty(&motor->pwm, motor->pwm.max_duty);
}

static const motor_ops_t s_motor_ops_tb6612 = {
    .enable = tb6612_enable,
    .disable = tb6612_disable,
    .set_power = tb6612_setpower,
    .brake = tb6612_brake,
    .read_speed_mps = NULL,
};

bsp_status_t Dev_Motor_TB6612_Init(motor_tb6612_t *motor, const motor_tb6612_cfg_t *cfg) {
  if (motor == NULL || cfg == NULL || cfg->name == NULL || !tb6612_pwm_isvalid(&cfg->pwm) ||
      !BSP_GPIO_IsValid(cfg->in1) || !BSP_GPIO_IsValid(cfg->in2) || cfg->encoder == NULL ||
      cfg->controller == NULL || !SYS_IS_POS_FINITE(cfg->counts_per_meter) ||
      !SYS_IS_POS_FINITE(cfg->control_period_s)) {
    return BSP_ERR_INVALID_ARG;
  }

  bool has_stby = (cfg->stby.port != NULL || cfg->stby.pin != 0U);
  if (has_stby && !BSP_GPIO_IsValid(cfg->stby)) {
    return BSP_ERR_INVALID_ARG;
  }

  uint32_t effective_max_duty =
      (cfg->pwm.max_duty > 0U) ? cfg->pwm.max_duty : (__HAL_TIM_GET_AUTORELOAD(cfg->pwm.htim) + 1U);
  if (effective_max_duty == 0U) {
    return BSP_ERR_INVALID_ARG;
  }

  bsp_pwm_t resolved_pwm = cfg->pwm;
  resolved_pwm.max_duty = effective_max_duty;

  if (!Dev_Motor_Base_Init(&motor->base, cfg->name, &s_motor_ops_tb6612, cfg->controller,
                           cfg->encoder, cfg->counts_per_meter, cfg->control_period_s,
                           effective_max_duty)) {
    return BSP_ERR_INVALID_ARG;
  }

  motor->pwm = resolved_pwm;
  motor->in1 = cfg->in1;
  motor->in2 = cfg->in2;
  motor->stby = cfg->stby;
  motor->last_direction = DEV_TB6612_DIR_COAST;

  BSP_GPIO_Write(motor->in1, false);
  BSP_GPIO_Write(motor->in2, false);
  if (has_stby) {
    BSP_GPIO_Write(motor->stby, false);
  }
  (void)BSP_PWM_SetDuty(&motor->pwm, 0U);

  bsp_status_t st = BSP_PWM_Start(&motor->pwm);
  if (st != BSP_OK) {
    if (has_stby) {
      BSP_GPIO_Write(motor->stby, false);
    }
    return st;
  }

  return BSP_OK;
}

void Dev_Motor_TB6612_EmergencyHalt(motor_tb6612_t *motor) {
  if (motor == NULL) {
    return;
  }
  if (tb6612_pwm_isvalid(&motor->pwm)) {
    (void)BSP_PWM_SetDuty(&motor->pwm, 0U);
  }
  BSP_GPIO_Write(motor->in1, false);
  BSP_GPIO_Write(motor->in2, false);
  if (BSP_GPIO_IsValid(motor->stby)) {
    BSP_GPIO_Write(motor->stby, false);
  }
  motor->last_direction = DEV_TB6612_DIR_COAST;
  motor->base.state = DEV_MOTOR_DISABLE;
  motor->base.target_speed_mps = 0.0f;
  motor->base.current_speed_mps = 0.0f;
}
