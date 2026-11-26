/**
 * @file chassis_omni.c
 * @brief 四轮全向底盘实现。
 * @ingroup chassis
 */

#include "chassis_omni.h"
#include "sys_angle.h"
#include "sys_math.h"
#include "sys_time_window.h"
#include <stddef.h>

static void chassis_omni_setmotion(chassis_base_t *chassis_h, const chassis_twist_t *twist) {
  chassis_omni_t *chassis = (chassis_omni_t *)chassis_h;
  if (chassis == NULL || twist == NULL) {
    return;
  }
  chassis->command.vx_body_mps = SYS_ISFINITE(twist->vx_body_mps) ? twist->vx_body_mps : 0.0f;
  chassis->command.vy_body_mps = SYS_ISFINITE(twist->vy_body_mps) ? twist->vy_body_mps : 0.0f;
  chassis->command.wz_radps = SYS_ISFINITE(twist->wz_radps) ? twist->wz_radps : 0.0f;
}

static void chassis_omni_stop(chassis_base_t *chassis_h) {
  chassis_omni_t *chassis = (chassis_omni_t *)chassis_h;
  if (chassis == NULL) {
    return;
  }
  chassis->command = (chassis_twist_t){0};
  for (uint8_t i = 0U; i < CHASSIS_OMNI_WHEEL_COUNT; ++i) {
    Dev_Motor_Base_SetTargetSpeed(chassis->motors[i], 0.0f);
    Dev_Motor_Base_Brake(chassis->motors[i]);
  }
}

static void chassis_omni_estop(chassis_base_t *chassis_h) {
  chassis_omni_t *chassis = (chassis_omni_t *)chassis_h;
  if (chassis == NULL) {
    return;
  }
  chassis->command = (chassis_twist_t){0};
  for (uint8_t i = 0U; i < CHASSIS_OMNI_WHEEL_COUNT; ++i) {
    Dev_Motor_Base_Disable(chassis->motors[i]);
  }
}

static void chassis_omni_update(chassis_base_t *chassis_h, uint32_t now_ms) {
  chassis_omni_t *chassis = (chassis_omni_t *)chassis_h;
  if (chassis == NULL || chassis->kinematics == NULL) {
    return;
  }

  float yaw_rad = 0.0f;
  if (chassis->frame == CHASSIS_FRAME_WORLD) {
    imu_sample_t imu_sample = {0};
    if (chassis->imu == NULL || !Dev_IMU_Base_GetSample(chassis->imu, &imu_sample) ||
        (imu_sample.valid_mask & IMU_CHANNEL_YAW) == 0U ||
        Sys_Time_IsExpiredMs(now_ms, imu_sample.channel_update_ms[IMU_CHANNEL_INDEX_YAW],
                             chassis->imu_timeout_ms)) {
      chassis_omni_stop(chassis_h);
      return;
    }
    yaw_rad = Sys_Angle_DegToRad(imu_sample.yaw_deg);
  }

  float wheel_mps[CHASSIS_OMNI_WHEEL_COUNT] = {0};
  Kinematics_Omni_Solve(chassis->kinematics, chassis->command.vx_body_mps,
                        chassis->command.vy_body_mps, chassis->command.wz_radps, chassis->frame,
                        yaw_rad, wheel_mps);

  for (uint8_t i = 0U; i < CHASSIS_OMNI_WHEEL_COUNT; ++i) {
    Dev_Motor_Base_SetTargetSpeed(chassis->motors[i], wheel_mps[i]);
  }
}

static const chassis_ops_t s_chassis_ops_omni = {
    .set_motion = chassis_omni_setmotion,
    .update = chassis_omni_update,
    .stop = chassis_omni_stop,
    .estop = chassis_omni_estop,
};

bool Chassis_Omni_Init(chassis_omni_t *chassis, const chassis_omni_cfg_t *cfg) {
  if (chassis == NULL || cfg == NULL || cfg->name == NULL || cfg->motors == NULL ||
      cfg->kinematics == NULL) {
    return false;
  }
  if (cfg->frame != CHASSIS_FRAME_BODY && cfg->frame != CHASSIS_FRAME_WORLD) {
    return false;
  }
  if (cfg->frame == CHASSIS_FRAME_WORLD && (cfg->imu == NULL || cfg->imu_timeout_ms == 0U)) {
    return false;
  }

  for (uint8_t i = 0U; i < CHASSIS_OMNI_WHEEL_COUNT; ++i) {
    if (cfg->motors[i] == NULL) {
      return false;
    }
    for (uint8_t j = 0U; j < i; ++j) {
      if (cfg->motors[i] == cfg->motors[j]) {
        return false;
      }
    }
  }

  Chassis_Base_Init(&chassis->base, cfg->name, &s_chassis_ops_omni);
  for (uint8_t i = 0U; i < CHASSIS_OMNI_WHEEL_COUNT; ++i) {
    chassis->motors[i] = cfg->motors[i];
  }
  chassis->kinematics = cfg->kinematics;
  chassis->imu = cfg->imu;
  chassis->frame = cfg->frame;
  chassis->command = (chassis_twist_t){0};
  chassis->imu_timeout_ms = cfg->imu_timeout_ms;
  return true;
}

void Chassis_Omni_SetMotion(chassis_omni_t *chassis, const chassis_twist_t *twist) {
  if (chassis == NULL) {
    return;
  }
  Chassis_Base_SetMotion(&chassis->base, twist);
}
