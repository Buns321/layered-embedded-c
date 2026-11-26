/**
 * @file kinematics_omni.c
 * @brief 四轮全向底盘扁平运动学实现。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，完全静态分配。
 *          提供初始化期三角函数预计算与运行期车体系/世界系（无头模式）四轮逆运动学解算，
 *          并在任意输入或中间结果非有限时执行四轮原子清零保护。
 * @ingroup Framework_Control
 */

#include "kinematics_omni.h"
#include "sys_math.h"
#include <math.h>
#include <stddef.h>

/**
 * @brief 将四轮输出线速度数组全部清零（用于边界异常或非法输入保护）。
 * @param[out] out_wheel_mps 四轮线速度输出数组（单位：m/s，非 NULL，长度 KINEMATICS_OMNI_WHEELS）。
 */
static void kinematics_omni_zero(float out_wheel_mps[KINEMATICS_OMNI_WHEELS]) {
  for (uint8_t i = 0U; i < KINEMATICS_OMNI_WHEELS; ++i) {
    out_wheel_mps[i] = 0.0f;
  }
}

bool Kinematics_Omni_Init(kinematics_omni_t *kin, const kinematics_omni_cfg_t *cfg) {
  if (kin == NULL || cfg == NULL || !SYS_IS_POS_FINITE(cfg->wheel_distance_m)) {
    return false;
  }

  // 先在栈上临时缓存预计算结果，全部四轮校验通过后再提交至 kin，保证失败不留半初始化状态
  float cos_tmp[KINEMATICS_OMNI_WHEELS];
  float sin_tmp[KINEMATICS_OMNI_WHEELS];
  float sign_tmp[KINEMATICS_OMNI_WHEELS];

  for (uint8_t i = 0U; i < KINEMATICS_OMNI_WHEELS; ++i) {
    if (!SYS_ISFINITE(cfg->wheel_theta_rad[i]) || !SYS_ISFINITE(cfg->wheel_sign[i])) {
      return false;
    }

    // 约定 wheel_sign == 0 时默认取 +1.0f，否则严格限定为 ±1.0f 防止隐式缩放错误
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
  // 边界保护：未初始化实例或平动/转动速度含 NaN/Inf 时四轮全部清零停机
  if (kin == NULL || !SYS_IS_POS_FINITE(kin->wheel_distance_m) || !SYS_ISFINITE(vx_mps) ||
      !SYS_ISFINITE(vy_mps) || !SYS_ISFINITE(wz_radps)) {
    kinematics_omni_zero(out_wheel_mps);
    return;
  }

  float vx_body_mps = 0.0f;
  float vy_body_mps = 0.0f;

  if (frame == CHASSIS_FRAME_BODY) {
    // 车体系直通：零三角函数运算，不依赖 IMU yaw_rad
    vx_body_mps = vx_mps;
    vy_body_mps = vy_mps;
  } else if (frame == CHASSIS_FRAME_WORLD) {
    // 世界系（无头模式）：将世界系平动速度按 -yaw_rad 旋转投影至车体系
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
    // 非法坐标系枚举值保护
    kinematics_omni_zero(out_wheel_mps);
    return;
  }

  // 全向轮投影逆解：v_i = (vx_body * cos(theta_i) + vy_body * sin(theta_i) + wz * R) * sign_i
  // 先写入临时数组，确保任一轮计算溢出时四轮输出原子清零而非部分更新
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
