/**
 * @file kinematics_omni.c
 * @brief 四轮全向底盘扁平运动学实现。
 * @ingroup control
 */

#include "kinematics_omni.h"
#include "sys_math.h"
#include <math.h>
#include <stddef.h>

static void kinematics_omni_zero(float out_wheel_mps[KINEMATICS_OMNI_WHEELS]) {
  for (uint8_t i = 0U; i < KINEMATICS_OMNI_WHEELS; ++i) {
    out_wheel_mps[i] = 0.0f;
  }
}

bool Kinematics_Omni_Init(kinematics_omni_t *kin, const kinematics_omni_cfg_t *cfg) {
  if (kin == NULL || cfg == NULL || !SYS_IS_POS_FINITE(cfg->wheel_distance_m)) {
    return false;
  }

  float cos_tmp[KINEMATICS_OMNI_WHEELS];
  float sin_tmp[KINEMATICS_OMNI_WHEELS];
  float sign_tmp[KINEMATICS_OMNI_WHEELS];

  for (uint8_t i = 0U; i < KINEMATICS_OMNI_WHEELS; ++i) {
    if (!SYS_ISFINITE(cfg->wheel_theta_rad[i]) || !SYS_ISFINITE(cfg->wheel_sign[i])) {
      return false;
    }

    float sign = (cfg->wheel_sign[i] == 0.0f) ? 1.0f : cfg->wheel_sign[i];
    if (sign != 1.0f && sign != -1.0f) {
      return false;
    }

    float c = cosf(cfg->wheel_theta_rad[i]);
    float s = sinf(cfg->wheel_theta_rad[i]);
    if (!SYS_ISFINITE(c) || !SYS_ISFINITE(s)) {
      return false;
    }

    cos_tmp[i] = c;
    sin_tmp[i] = s;
    sign_tmp[i] = sign;
  }

  kin->wheel_distance_m = cfg->wheel_distance_m;
  for (uint8_t i = 0U; i < KINEMATICS_OMNI_WHEELS; ++i) {
    kin->cos_theta[i] = cos_tmp[i];
    kin->sin_theta[i] = sin_tmp[i];
    kin->wheel_sign[i] = sign_tmp[i];
  }
  return true;
}

void Kinematics_Omni_Solve(const kinematics_omni_t *kin, float vx_mps, float vy_mps, float wz_radps,
                           chassis_frame_t frame, float yaw_rad,
                           float out_wheel_mps[KINEMATICS_OMNI_WHEELS]) {
  if (out_wheel_mps == NULL) {
    return;
  }
  if (kin == NULL || !SYS_IS_POS_FINITE(kin->wheel_distance_m) || !SYS_ISFINITE(vx_mps) ||
      !SYS_ISFINITE(vy_mps) || !SYS_ISFINITE(wz_radps)) {
    kinematics_omni_zero(out_wheel_mps);
    return;
  }

  float vx_body_mps = 0.0f;
  float vy_body_mps = 0.0f;

  if (frame == CHASSIS_FRAME_BODY) {
    vx_body_mps = vx_mps;
    vy_body_mps = vy_mps;
  } else if (frame == CHASSIS_FRAME_WORLD) {
    if (!SYS_ISFINITE(yaw_rad)) {
      kinematics_omni_zero(out_wheel_mps);
      return;
    }
    float cos_yaw = cosf(yaw_rad);
    float sin_yaw = sinf(yaw_rad);
    if (!SYS_ISFINITE(cos_yaw) || !SYS_ISFINITE(sin_yaw)) {
      kinematics_omni_zero(out_wheel_mps);
      return;
    }
    vx_body_mps = (vx_mps * cos_yaw) + (vy_mps * sin_yaw);
    vy_body_mps = (-vx_mps * sin_yaw) + (vy_mps * cos_yaw);
  } else {
    kinematics_omni_zero(out_wheel_mps);
    return;
  }

  float out_tmp[KINEMATICS_OMNI_WHEELS];
  for (uint8_t i = 0U; i < KINEMATICS_OMNI_WHEELS; ++i) {
    float trans = (vx_body_mps * kin->cos_theta[i]) + (vy_body_mps * kin->sin_theta[i]);
    float rotate = wz_radps * kin->wheel_distance_m;
    float wheel_mps = (trans + rotate) * kin->wheel_sign[i];
    if (!SYS_ISFINITE(wheel_mps)) {
      kinematics_omni_zero(out_wheel_mps);
      return;
    }
    out_tmp[i] = wheel_mps;
  }

  for (uint8_t i = 0U; i < KINEMATICS_OMNI_WHEELS; ++i) {
    out_wheel_mps[i] = out_tmp[i];
  }
}
