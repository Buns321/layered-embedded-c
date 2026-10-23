//
// 由 Buns_ 创建于 2026/8/24。
//

#include "motor_base.h"
#include "controller_base.h"
#include "encoder_base.h"
#include "system_math.h"
#include <assert.h>

void Dev_Motor_Base_Init(
  motor_base_t *this,
  const char *name,
  const motor_ops_t *ops,
  controller_base_t *controller,
  encoder_base_t *encoder,
  float counts_per_meter,
  float control_period,
  float target_speed,
  int32_t max_duty
) {
  assert(
    this                           &&
    name                           &&
    ops                            &&
    controller                     &&
    encoder                        &&
    SYS_ISFINITE(counts_per_meter) &&
    counts_per_meter > 0.0f        &&
    SYS_ISFINITE(control_period)   &&
    control_period > 0.0f          &&
    max_duty > 0
  );

  this->ops              = ops;
  this->name             = name;
  this->controller       = controller;
  this->encoder          = encoder;
  this->state            = DEV_MOTOR_DISABLE;
  this->current_speed    = 0;
  this->target_speed     = 0.0f;
  this->counts_per_meter = counts_per_meter;
  this->control_period   = control_period;
  this->max_duty         = max_duty;
  this->controller_status = CONTROLLER_STATUS_OK;

  Dev_Motor_Base_SetTargetSpeed(this, target_speed);
}

Dev_Motor_State Dev_Motor_Base_ReadStatus(motor_base_t *this) {
  assert(this);
  return this->state;
}

float Dev_Motor_Base_ReadTargetSpeed(motor_base_t *this) {
  assert(this);
  return this->target_speed;
}

float Dev_Motor_Base_ReadCurrentSpeed(motor_base_t *this) {
  assert(this);
  return this->current_speed;
}

controller_status_t Dev_Motor_Base_ReadControllerStatus(motor_base_t *this) {
  assert(this);
  return this->controller_status;
}

void Dev_Motor_Base_SetTargetSpeed(motor_base_t *this, float target_speed) {
  assert(this && SYS_ISFINITE(target_speed));

  if (!SYS_ISFINITE(target_speed)) {
    target_speed = 0.0f;
  }

  this->target_speed = target_speed;
}

void Dev_Motor_Base_Enable(motor_base_t *this) {
  assert(
    this              &&
    this->ops         &&
    this->ops->enable
  );

  Dev_Motor_Base_Reset(this);
  this->ops->enable(this);
  this->state            = DEV_MOTOR_ENABLE;
}


void Dev_Motor_Base_Disable(motor_base_t *this) {
  assert(
    this                 &&
    this->ops            &&
    this->ops->set_power &&
    this->ops->disable
  );

  this->ops->set_power(this, 0.0f);
  this->ops->disable(this);

  this->state            = DEV_MOTOR_DISABLE;
  this->current_speed = 0.0f;
  this->target_speed = 0.0f;
  this->controller_status = CONTROLLER_STATUS_OK;
}

void Dev_Motor_Base_Brake(motor_base_t *this) {
  assert(
    this             &&
    this->ops        &&
    this->ops->brake &&
    this->controller
  );

  this->target_speed  = 0.0f;
  this->current_speed = 0.0f;
  this->controller_status = CONTROLLER_STATUS_OK;

  if (this->state == DEV_MOTOR_DISABLE) {
    return;
  }

  Ctrl_Base_Reset(this->controller);
  this->ops->brake(this);
  this->state            = DEV_MOTOR_BRAKE;
}

void Dev_Motor_Base_Update(motor_base_t *this) {
  assert(
    this                 &&
    this->ops            &&
    this->ops->set_power &&
    this->controller     &&
    this->encoder
  );

  if (this->state != DEV_MOTOR_ENABLE) {
    return;
  }

  int32_t delta = Dev_Encoder_Base_GetDelta(this->encoder);

  float speed_scale = this->counts_per_meter * this->control_period;
  if (!SYS_ISFINITE(speed_scale) || speed_scale <= 0.0f) {
    this->current_speed = 0.0f;
    this->controller_status = CONTROLLER_STATUS_ERROR;
    this->ops->set_power(this, 0.0f);
    return;
  }

  this->current_speed = (float)delta / speed_scale;

  float power = 0.0f;
  controller_status_t status = Ctrl_Base_Update(
    this->controller,
    this->target_speed,
    this->current_speed,
    this->control_period,
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
    -(float)this->max_duty,
    (float)this->max_duty
  );

  if (clamped_power != power) {
    status = CONTROLLER_STATUS_SATURATED;
  }

  this->controller_status = status;
  this->ops->set_power(this, clamped_power);
}

void Dev_Motor_Base_Reset(motor_base_t *this) {
  assert(
    this                 &&
    this->ops            &&
    this->ops->set_power &&
    this->controller     &&
    this->encoder
  );

  this->ops->set_power(this, 0.0f);

  Ctrl_Base_Reset(this->controller);

  Dev_Encoder_Base_Resync(this->encoder);

  this->current_speed = 0.0f;
  this->controller_status = CONTROLLER_STATUS_OK;
}
