//
// 由 Buns_ 创建于 2026/8/24。
//

/**
 * @file motor_base.c
 * @brief 闭环电机抽象接口实现。
 * @ingroup drivers_base
 */

#include "motor_base.h"
#include "controller_base.h"
#include "encoder_base.h"
#include "system_math.h"
#include <assert.h>

void Dev_Motor_Base_Init(
  motor_base_t *motor_h,
  const char *name,
  const motor_ops_t *ops,
  controller_base_t *controller_h,
  encoder_base_t *encoder_h,
  float counts_per_meter,
  float control_period,
  float target_speed,
  int32_t max_duty
) {
  assert(
    motor_h                           &&
    name                           &&
    ops                            &&
    controller_h                     &&
    encoder_h                        &&
    SYS_ISFINITE(counts_per_meter) &&
    counts_per_meter > 0.0f        &&
    SYS_ISFINITE(control_period)   &&
    control_period > 0.0f          &&
    max_duty > 0
  );

  motor_h->ops              = ops;
  motor_h->name             = name;
  motor_h->controller_h       = controller_h;
  motor_h->encoder_h          = encoder_h;
  motor_h->state            = DEV_MOTOR_DISABLE;
  motor_h->current_speed    = 0;
  motor_h->target_speed     = 0.0f;
  motor_h->counts_per_meter = counts_per_meter;
  motor_h->control_period   = control_period;
  motor_h->max_duty         = max_duty;
  motor_h->controller_status = CONTROLLER_STATUS_OK;

  Dev_Motor_Base_SetTargetSpeed(motor_h, target_speed);
}

dev_motor_state_t Dev_Motor_Base_ReadStatus(motor_base_t *motor_h) {
  assert(motor_h);
  return motor_h->state;
}

float Dev_Motor_Base_ReadTargetSpeed(motor_base_t *motor_h) {
  assert(motor_h);
  return motor_h->target_speed;
}

float Dev_Motor_Base_ReadCurrentSpeed(motor_base_t *motor_h) {
  assert(motor_h);
  return motor_h->current_speed;
}

controller_status_t Dev_Motor_Base_ReadControllerStatus(motor_base_t *motor_h) {
  assert(motor_h);
  return motor_h->controller_status;
}

void Dev_Motor_Base_SetTargetSpeed(motor_base_t *motor_h, float target_speed) {
  assert(motor_h && SYS_ISFINITE(target_speed));

  if (!SYS_ISFINITE(target_speed)) {
    target_speed = 0.0f;
  }

  motor_h->target_speed = target_speed;
}

void Dev_Motor_Base_Enable(motor_base_t *motor_h) {
  assert(
    motor_h              &&
    motor_h->ops         &&
    motor_h->ops->enable
  );

  Dev_Motor_Base_Reset(motor_h);
  motor_h->ops->enable(motor_h);
  motor_h->state            = DEV_MOTOR_ENABLE;
}


void Dev_Motor_Base_Disable(motor_base_t *motor_h) {
  assert(
    motor_h                 &&
    motor_h->ops            &&
    motor_h->ops->set_power &&
    motor_h->ops->disable
  );

  motor_h->ops->set_power(motor_h, 0.0f);
  motor_h->ops->disable(motor_h);

  motor_h->state            = DEV_MOTOR_DISABLE;
  motor_h->current_speed = 0.0f;
  motor_h->target_speed = 0.0f;
  motor_h->controller_status = CONTROLLER_STATUS_OK;
}

void Dev_Motor_Base_Brake(motor_base_t *motor_h) {
  assert(
    motor_h             &&
    motor_h->ops        &&
    motor_h->ops->brake &&
    motor_h->controller_h
  );

  motor_h->target_speed  = 0.0f;
  motor_h->current_speed = 0.0f;
  motor_h->controller_status = CONTROLLER_STATUS_OK;

  if (motor_h->state == DEV_MOTOR_DISABLE) {
    return;
  }

  Ctrl_Base_Reset(motor_h->controller_h);
  motor_h->ops->brake(motor_h);
  motor_h->state            = DEV_MOTOR_BRAKE;
}

void Dev_Motor_Base_Update(motor_base_t *motor_h) {
  assert(
    motor_h                 &&
    motor_h->ops            &&
    motor_h->ops->set_power &&
    motor_h->controller_h     &&
    motor_h->encoder_h
  );

  if (motor_h->state != DEV_MOTOR_ENABLE) {
    return;
  }

  int32_t delta = Dev_Encoder_Base_GetDelta(motor_h->encoder_h);

  float speed_scale = motor_h->counts_per_meter * motor_h->control_period;
  if (!SYS_ISFINITE(speed_scale) || speed_scale <= 0.0f) {
    motor_h->current_speed = 0.0f;
    motor_h->controller_status = CONTROLLER_STATUS_ERROR;
    motor_h->ops->set_power(motor_h, 0.0f);
    return;
  }

  motor_h->current_speed = (float)delta / speed_scale;

  float power = 0.0f;
  controller_status_t status = Ctrl_Base_Update(
    motor_h->controller_h,
    motor_h->target_speed,
    motor_h->current_speed,
    motor_h->control_period,
    &power
  );

  if (!SYS_ISFINITE(power)) {
    power = 0.0f;
    status = CONTROLLER_STATUS_ERROR;
  } else if (status == CONTROLLER_STATUS_ERROR) {
    power = 0.0f;
  }

  float clamped_power = SYS_CLAMP(
    power,
    -(float)motor_h->max_duty,
    (float)motor_h->max_duty
  );

  if (clamped_power != power) {
    status = CONTROLLER_STATUS_SATURATED;
  }

  motor_h->controller_status = status;
  motor_h->ops->set_power(motor_h, clamped_power);
}

void Dev_Motor_Base_Reset(motor_base_t *motor_h) {
  assert(
    motor_h                 &&
    motor_h->ops            &&
    motor_h->ops->set_power &&
    motor_h->controller_h     &&
    motor_h->encoder_h
  );

  motor_h->ops->set_power(motor_h, 0.0f);

  Ctrl_Base_Reset(motor_h->controller_h);

  Dev_Encoder_Base_Resync(motor_h->encoder_h);

  motor_h->current_speed = 0.0f;
  motor_h->controller_status = CONTROLLER_STATUS_OK;
}
