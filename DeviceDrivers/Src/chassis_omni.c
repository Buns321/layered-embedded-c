//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file chassis_omni.c
 * @brief Omni 底盘实现。
 * @ingroup chassis
 */

#include "chassis_omni.h"
#include "system_math.h"
#include <assert.h>

static void Chassis_Omni_Stop(chassis_base_t *chassis_h);

static void Chassis_Omni_Update(chassis_base_t *chassis_h, uint32_t now_ms) {
  assert(chassis_h);

  chassis_omni_t *chassis_omni_h = (chassis_omni_t *)chassis_h;
  assert(chassis_omni_h->imu_h && chassis_omni_h->policy_h);

  imu_sample_t imu_sample = {0};
  bool imu_available = Dev_IMU_Base_GetSample(
    chassis_omni_h->imu_h,
    &imu_sample
  );

  uint32_t yaw_age_ms =
    now_ms -
    imu_sample.channel_update_ms[IMU_CHANNEL_INDEX_YAW];

  if (
    !imu_available                                          ||
    (imu_sample.valid_mask & IMU_CHANNEL_YAW) == 0u         ||
    yaw_age_ms > chassis_omni_h->imu_timeout_ms
  ) {
    Chassis_Omni_Stop(chassis_h);
    return;
  }

  const float deg_to_rad = 3.14159265358979323846f / 180.0f;
  float yaw_rad = imu_sample.yaw_deg * deg_to_rad;
  float wheel_speed[CHASSIS_OMNI_WHEEL_COUNT] = {0};

  Policy_Omni_Base_Solve(
    chassis_omni_h->policy_h,
    chassis_omni_h->v,
    chassis_omni_h->dir,
    yaw_rad,
    chassis_omni_h->omega,
    wheel_speed
  );

  for (uint8_t i = 0; i < CHASSIS_OMNI_WHEEL_COUNT; i++) {
    assert(chassis_omni_h->motor_h[i]);
    Dev_Motor_Base_SetTargetSpeed(chassis_omni_h->motor_h[i], wheel_speed[i]);
  }
}

static void Chassis_Omni_Stop(chassis_base_t *chassis_h) {
  assert(chassis_h);

  chassis_omni_t *chassis_omni_h = (chassis_omni_t *)chassis_h;

  chassis_omni_h->v     = 0.0f;
  chassis_omni_h->omega = 0.0f;

  for (uint8_t i = 0; i < CHASSIS_OMNI_WHEEL_COUNT; i++) {
    assert(chassis_omni_h->motor_h[i]);
    Dev_Motor_Base_SetTargetSpeed(chassis_omni_h->motor_h[i], 0.0f);
    Dev_Motor_Base_Brake(chassis_omni_h->motor_h[i]);
  }
}

static void Chassis_Omni_EStop(chassis_base_t *chassis_h) {
  assert(chassis_h);

  chassis_omni_t *chassis_omni_h = (chassis_omni_t *)chassis_h;

  chassis_omni_h->v     = 0.0f;
  chassis_omni_h->omega = 0.0f;

  for (uint8_t i = 0; i < CHASSIS_OMNI_WHEEL_COUNT; i++) {
    assert(chassis_omni_h->motor_h[i]);
    Dev_Motor_Base_Disable(chassis_omni_h->motor_h[i]);
  }
}

static const chassis_ops_t chassis_ops_omni = {
  .update = Chassis_Omni_Update,
  .stop   = Chassis_Omni_Stop,
  .estop  = Chassis_Omni_EStop,
};

void Chassis_Omni_Init(
  chassis_omni_t     *chassis_h,
  const char         *name,
  motor_base_t *const motor_h[CHASSIS_OMNI_WHEEL_COUNT],
  imu_base_t         *imu_h,
  policy_omni_base_t *policy_h,
  uint32_t            imu_timeout_ms
) {
  assert(
    chassis_h            &&
    name            &&
    motor_h           &&
    imu_h             &&
    policy_h          &&
    imu_timeout_ms > 0u
  );

  for (uint8_t i = 0; i < CHASSIS_OMNI_WHEEL_COUNT; i++) {
    assert(motor_h[i]);

    for (uint8_t j = 0; j < i; j++) {
      assert(motor_h[i] != motor_h[j]);
    }
  }

  Chassis_Base_Init(&chassis_h->base, name, &chassis_ops_omni);

  for (uint8_t i = 0; i < CHASSIS_OMNI_WHEEL_COUNT; i++) {
    chassis_h->motor_h[i] = motor_h[i];
  }

  chassis_h->imu_h    = imu_h;
  chassis_h->policy_h = policy_h;
  chassis_h->v      = 0.0f;
  chassis_h->dir    = 0.0f;
  chassis_h->omega  = 0.0f;
  chassis_h->imu_timeout_ms = imu_timeout_ms;
}

void Chassis_Omni_SetMotion(chassis_omni_t *chassis_h, float v, float dir, float omega) {
  assert(chassis_h);

  assert(
    SYS_ISFINITE(v)     &&
    SYS_ISFINITE(dir)   &&
    SYS_ISFINITE(omega)
  );

  if (!SYS_ISFINITE(v))     v     = 0.0f;
  if (!SYS_ISFINITE(dir))   dir   = 0.0f;
  if (!SYS_ISFINITE(omega)) omega = 0.0f;

  chassis_h->v     = v;
  chassis_h->dir   = dir;
  chassis_h->omega = omega;
}
