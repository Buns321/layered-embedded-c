/**
 * @file sys_angle.h
 * @brief 角度单位换算与有限角度规范周期归一化。
 * @addtogroup sys_core
 * @{
 */

#ifndef LAYERED_CAR_SYS_ANGLE_H
#define LAYERED_CAR_SYS_ANGLE_H

#include <stdbool.h>

/** @brief 单精度圆周率常量（无量纲）。 */
#define SYS_PI 3.14159265358979323846f

/**
 * @brief 将角度从 deg 换算为 rad（不做周期归一化）。
 */
static inline float Sys_Angle_DegToRad(float angle_deg) { return angle_deg * (SYS_PI / 180.0f); }

/**
 * @brief 将角度从 rad 换算为 deg（不做周期归一化）。
 */
static inline float Sys_Angle_RadToDeg(float angle_rad) { return angle_rad * (180.0f / SYS_PI); }

/**
 * @brief 把有限角度归一化到 [-180, 180] deg；非有限或空指针返回 false 且不写输出。
 */
bool Sys_Angle_WrapDegrees(float angle_deg, float *wrapped_deg);

/**
 * @brief 把有限角度归一化到 [-pi, pi] rad；非有限或空指针返回 false 且不写输出。
 */
bool Sys_Angle_WrapRadians(float angle_rad, float *wrapped_rad);

/** @} */

#endif /* LAYERED_CAR_SYS_ANGLE_H */
