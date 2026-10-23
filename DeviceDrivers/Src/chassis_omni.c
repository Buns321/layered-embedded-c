//
// 由 Buns_ 创建于 2026/10/20。
//

#include "chassis_omni.h"
#include "system_math.h"
#include <assert.h>

static void Chassis_Omni_Stop(chassis_base_t *this);

static void Chassis_Omni_Update(chassis_base_t *this, uint32_t now_ms) {
  assert(this);

  chassis_omni_t *self = (chassis_omni_t *)this;
  assert(self->imu && self->policy);

  imu_sample_t imu_sample = {0};
  bool imu_available = Dev_IMU_Base_GetSample(
    self->imu,
    &imu_sample
  );

  uint32_t yaw_age_ms =
    now_ms -
    imu_sample.channel_update_ms[IMU_CHANNEL_INDEX_YAW];

  if (
    !imu_available                                          ||
    (imu_sample.valid_mask & IMU_CHANNEL_YAW) == 0u         ||
    yaw_age_ms > self->imu_timeout_ms
  ) {
    Chassis_Omni_Stop(this);
    return;
  }

  const float deg_to_rad = 3.14159265358979323846f / 180.0f;
  float yaw_rad = imu_sample.yaw_deg * deg_to_rad;
  float wheel_speed[CHASSIS_OMNI_WHEEL_COUNT] = {0};

  Omni_Policy_Base_Solve(
    self->policy,
    self->v,
    self->dir,
    yaw_rad,
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
  omni_policy_base_t *policy,
  uint32_t            imu_timeout_ms
) {
  assert(
    this            &&
    name            &&
    wheel           &&
    imu             &&
    policy          &&
    imu_timeout_ms > 0u
  );

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
  this->imu_timeout_ms = imu_timeout_ms;
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
