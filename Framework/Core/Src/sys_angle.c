/**
 * @file sys_angle.c
 * @brief 有限角度周期归一化实现。
 * @ingroup sys_core
 */

#include "sys_angle.h"
#include "sys_math.h"
#include <math.h>
#include <stddef.h>

static float sys_angle_wrapdegreesfinite(float angle_deg) {
  float wrapped_deg = fmodf(angle_deg, 360.0f);
  if (wrapped_deg > 180.0f) {
    wrapped_deg -= 360.0f;
  } else if (wrapped_deg < -180.0f) {
    wrapped_deg += 360.0f;
  }
  return wrapped_deg;
}

static float sys_angle_wrapradiansfinite(float angle_rad) {
  const float period_rad = 2.0f * SYS_PI;
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
