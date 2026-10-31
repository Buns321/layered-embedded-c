/**
 * @file system_angle.c
 * @brief 有限角度归一化实现。
 * @ingroup system
 */

#include "system_angle.h"
#include "system_check.h"
#include <math.h>
#include <stddef.h>

#define SYS_ANGLE_PI 3.14159265358979323846f

/**
 * @brief 把已知有限角度归一化到 [-180, 180] deg。
 * @param angle_deg 待归一化角度，单位 deg；必须有限。
 * @return 位于 [-180, 180] deg 的等价角。
 * @pre angle_deg 必须有限。
 * @post 不修改输入或共享状态。
 * @note 私有辅助函数，使用 fmodf 先缩小范围；不分配内存。
 */
static float sys_angle_wrapdegreesfinite(float angle_deg) {
  float wrapped_deg = fmodf(angle_deg, 360.0f);
  if (wrapped_deg > 180.0f) {
    wrapped_deg -= 360.0f;
  } else if (wrapped_deg < -180.0f) {
    wrapped_deg += 360.0f;
  }
  return wrapped_deg;
}

/**
 * @brief 把已知有限角度归一化到 [-pi, pi] rad。
 * @param angle_rad 待归一化角度，单位 rad；必须有限。
 * @return 位于 [-pi, pi] rad 的等价角。
 * @pre angle_rad 必须有限。
 * @post 不修改输入或共享状态。
 * @note 私有辅助函数，使用 fmodf 先缩小范围；不分配内存。
 */
static float sys_angle_wrapradiansfinite(float angle_rad) {
  const float period_rad = 2.0f * SYS_ANGLE_PI;
  float wrapped_rad = fmodf(angle_rad, period_rad);
  if (wrapped_rad > SYS_ANGLE_PI) {
    wrapped_rad -= period_rad;
  } else if (wrapped_rad < -SYS_ANGLE_PI) {
    wrapped_rad += period_rad;
  }
  return wrapped_rad;
}

/**
 * @brief 把有限角度归一化到 [-180, 180] deg。
 * @param angle_deg 待归一化角度，单位 deg；必须为有限值。
 * @param wrapped_deg 输出角度，单位 deg；不得为 NULL，仅成功时写入。
 * @retval true angle_deg 有限，输出为其模 360 deg 的等价角。
 * @retval false 指针为空或 angle_deg 非有限，输出保持不变。
 * @pre wrapped_deg 可写。
 * @post 成功输出位于 [-180, 180]，正负 180 deg 原样保留。
 * @note 不分配内存；可在任务或 ISR 上下文调用。
 */
bool Sys_Angle_WrapDegrees(float angle_deg, float *wrapped_deg) {
  if (wrapped_deg == NULL || !SYS_ISFINITE(angle_deg)) {
    return false;
  }
  *wrapped_deg = sys_angle_wrapdegreesfinite(angle_deg);
  return true;
}

/**
 * @brief 把有限角度归一化到 [-pi, pi] rad。
 * @param angle_rad 待归一化角度，单位 rad；必须为有限值。
 * @param wrapped_rad 输出角度，单位 rad；不得为 NULL，仅成功时写入。
 * @retval true angle_rad 有限，输出为其模 2*pi 的等价角。
 * @retval false 指针为空或 angle_rad 非有限，输出保持不变。
 * @pre wrapped_rad 可写。
 * @post 成功输出位于 [-pi, pi]。
 * @note 不分配内存；可在任务或 ISR 上下文调用。
 */
bool Sys_Angle_WrapRadians(float angle_rad, float *wrapped_rad) {
  if (wrapped_rad == NULL || !SYS_ISFINITE(angle_rad)) {
    return false;
  }
  *wrapped_rad = sys_angle_wrapradiansfinite(angle_rad);
  return true;
}
