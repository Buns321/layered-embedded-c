//
// Created by Buns_ on 2026/10/20.
//

#include "chassis_omni.h"
#include "system_math.h"
#include <assert.h>

static void Chassis_Omni_Update(chassis_base_t *this) {
  assert(this);

  chassis_omni_t *self = (chassis_omni_t *)this;
  assert(self->imu && self->policy);

  float yaw = Dev_IMU_Base_GetYaw(self->imu);
  float wheel_speed[CHASSIS_OMNI_WHEEL_COUNT] = {0};

  Omni_Policy_Base_Solve(
    self->policy,
    self->v,
    self->dir,
    yaw,
    self->omega,
    wheel_speed
  );

  for (uint8_t i = 0; i < CHASSIS_OMNI_WHEEL_COUNT; i++) {
    assert(self->wheel[i]);
    Dev_Motor_Base_SetTargetSpeed(self->wheel[i], wheel_speed[i]);
  }
}

static void Chassis_Omni_Stop(chassis_base_t *this) {
  assert(this);

  chassis_omni_t *self = (chassis_omni_t *)this;

  self->v     = 0.0f;
  self->omega = 0.0f;

  for (uint8_t i = 0; i < CHASSIS_OMNI_WHEEL_COUNT; i++) {
    assert(self->wheel[i]);
    Dev_Motor_Base_SetTargetSpeed(self->wheel[i], 0.0f);
    Dev_Motor_Base_Brake(self->wheel[i]);
  }
}

static void Chassis_Omni_EStop(chassis_base_t *this) {
  assert(this);

  chassis_omni_t *self = (chassis_omni_t *)this;

  self->v     = 0.0f;
  self->omega = 0.0f;

  for (uint8_t i = 0; i < CHASSIS_OMNI_WHEEL_COUNT; i++) {
    assert(self->wheel[i]);
    Dev_Motor_Base_Disable(self->wheel[i]);
  }
}

static const chassis_ops_t chassis_ops_omni = {
  .update = Chassis_Omni_Update,
  .stop   = Chassis_Omni_Stop,
  .estop  = Chassis_Omni_EStop,
};

void Chassis_Omni_Init(
  chassis_omni_t     *this,
  const char         *name,
  motor_base_t *const wheel[CHASSIS_OMNI_WHEEL_COUNT],
  imu_base_t         *imu,
  omni_policy_base_t *policy
) {
  assert(this && name && wheel && imu && policy);

  for (uint8_t i = 0; i < CHASSIS_OMNI_WHEEL_COUNT; i++) {
    assert(wheel[i]);

    for (uint8_t j = 0; j < i; j++) {
      assert(wheel[i] != wheel[j]);
    }
  }

  Chassis_Base_Init(&this->base, name, &chassis_ops_omni);

  for (uint8_t i = 0; i < CHASSIS_OMNI_WHEEL_COUNT; i++) {
    this->wheel[i] = wheel[i];
  }

  this->imu    = imu;
  this->policy = policy;
  this->v      = 0.0f;
  this->dir    = 0.0f;
  this->omega  = 0.0f;
}

void Chassis_Omni_SetMotion(chassis_omni_t *this, float v, float dir, float omega) {
  assert(this);

  assert(
    SYS_ISFINITE(v)     &&
    SYS_ISFINITE(dir)   &&
    SYS_ISFINITE(omega)
  );

  if (!SYS_ISFINITE(v))     v     = 0.0f;
  if (!SYS_ISFINITE(dir))   dir   = 0.0f;
  if (!SYS_ISFINITE(omega)) omega = 0.0f;

  this->v     = v;
  this->dir   = dir;
  this->omega = omega;
}
