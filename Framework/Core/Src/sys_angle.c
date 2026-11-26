/**
 * @file sys_angle.c
 * @brief 有限角度周期归一化实现。
 * @details 属于 Layer 2（Framework/Core），先对输入执行 SYS_ISFINITE 有限值校验，
 *          再基于 fmodf 与单次区间折叠将角度映射至对称主值区间 [-180, 180] deg 或 [-pi, pi] rad。
 * @ingroup Framework_Core
 */

#include "sys_angle.h"
#include "sys_math.h"
#include <math.h>
#include <stddef.h>

/**
 * @brief 将已知有限的角度折叠到 [-180, 180] deg 主值区间。
 * @param[in] angle_deg 有限输入角度（单位：deg）。
 * @return 归一化后的角度（单位：deg）。
 */
static float sys_angle_wrapdegreesfinite(float angle_deg) {
  // fmodf 结果范围为 (-360, 360)，仅需单次加减 360 即可落入 [-180, 180]
  float wrapped_deg = fmodf(angle_deg, 360.0f);
  if (wrapped_deg > 180.0f) {
    wrapped_deg -= 360.0f;
  } else if (wrapped_deg < -180.0f) {
    wrapped_deg += 360.0f;
  }
  return wrapped_deg;
}

/**
 * @brief 将已知有限的弧度折叠到 [-pi, pi] rad 主值区间。
 * @param[in] angle_rad 有限输入弧度（单位：rad）。
 * @return 归一化后的弧度（单位：rad）。
 */
static float sys_angle_wrapradiansfinite(float angle_rad) {
  const float period_rad = 2.0f * SYS_PI;
  // fmodf 结果范围为 (-2*pi, 2*pi)，仅需单次加减 2*pi 即可落入 [-pi, pi]
  float wrapped_rad = fmodf(angle_rad, period_rad);
  if (wrapped_rad > SYS_PI) {
    wrapped_rad -= period_rad;
  } else if (wrapped_rad < -SYS_PI) {
    wrapped_rad += period_rad;
  }
  return wrapped_rad;
}

bool Sys_Angle_WrapDegrees(float angle_deg, float *wrapped_deg) {
  if (wrapped_deg == NULL || !SYS_ISFINITE(angle_deg)) {
    return false;
  }
  *wrapped_deg = sys_angle_wrapdegreesfinite(angle_deg);
  return true;
}

bool Sys_Angle_WrapRadians(float angle_rad, float *wrapped_rad) {
  if (wrapped_rad == NULL || !SYS_ISFINITE(angle_rad)) {
    return false;
  }
  *wrapped_rad = sys_angle_wrapradiansfinite(angle_rad);
  return true;
}
