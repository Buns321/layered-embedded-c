/**
 * @file sys_angle.h
 * @brief 角度单位换算与有限角度规范周期归一化。
 * @details 属于 Layer 2（Framework/Core），纯数学与单位换算工具，零 HAL/RTOS 依赖，
 *          可供 Control 层与 Drivers 层安全包含。
 * @addtogroup Framework_Core
 * @{
 */

#ifndef LAYERED_CAR_SYS_ANGLE_H
#define LAYERED_CAR_SYS_ANGLE_H

#include <stdbool.h>

/** @brief 单精度圆周率常量（无量纲）。 */
#define SYS_PI 3.14159265358979323846f

/**
 * @brief 将角度从 deg 换算为 rad（不做周期归一化）。
 * @param[in] angle_deg 输入角度（单位：deg）。
 * @return 换算后的弧度值（单位：rad）。
 */
static inline float Sys_Angle_DegToRad(float angle_deg) { return angle_deg * (SYS_PI / 180.0f); }

/**
 * @brief 将角度从 rad 换算为 deg（不做周期归一化）。
 * @param[in] angle_rad 输入弧度（单位：rad）。
 * @return 换算后的角度值（单位：deg）。
 */
static inline float Sys_Angle_RadToDeg(float angle_rad) { return angle_rad * (180.0f / SYS_PI); }

/**
 * @brief 把有限角度归一化到 [-180, 180] deg。
 * @param[in]  angle_deg   待归一化的输入角度（单位：deg，须为有限值）。
 * @param[out] wrapped_deg 归一化结果输出指针（单位：deg，不可为 NULL）。
 * @return 归一化是否成功。
 * @retval true  输入有限且已写入 [-180, 180] 区间结果。
 * @retval false 输入非有限（NaN/Inf）或输出指针为 NULL（不修改输出）。
 */
bool Sys_Angle_WrapDegrees(float angle_deg, float *wrapped_deg);

/**
 * @brief 把有限角度归一化到 [-pi, pi] rad。
 * @param[in]  angle_rad   待归一化的输入弧度（单位：rad，须为有限值）。
 * @param[out] wrapped_rad 归一化结果输出指针（单位：rad，不可为 NULL）。
 * @return 归一化是否成功。
 * @retval true  输入有限且已写入 [-pi, pi] 区间结果。
 * @retval false 输入非有限（NaN/Inf）或输出指针为 NULL（不修改输出）。
 */
bool Sys_Angle_WrapRadians(float angle_rad, float *wrapped_rad);

/** @} */

#endif /* LAYERED_CAR_SYS_ANGLE_H */
