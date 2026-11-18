/**
 * @file system_angle.h
 * @brief 有限角度到规范周期的归一化。
 * @addtogroup system
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_ANGLE_H
#define LAYERED_EMBEDDED_C_SYSTEM_ANGLE_H

#include <stdbool.h>

/** @brief 单精度圆周率常量（无量纲）。 */
#define SYS_PI 3.14159265358979323846f

/**
 * @brief 将角度从度换算为弧度。
 * @param angle_deg 角度值，单位 deg。
 * @return 对应的弧度值，单位 rad。
 * @post 纯函数，不修改共享状态。
 * @note 不做范围归一化；可在任务或 ISR 中调用。
 */
static inline float Sys_Angle_DegToRad(float angle_deg) { return angle_deg * (SYS_PI / 180.0f); }

/**
 * @brief 将角度从弧度换算为度。
 * @param angle_rad 角度值，单位 rad。
 * @return 对应的角度值，单位 deg。
 * @post 纯函数，不修改共享状态。
 * @note 不做范围归一化；可在任务或 ISR 中调用。
 */
static inline float Sys_Angle_RadToDeg(float angle_rad) { return angle_rad * (180.0f / SYS_PI); }

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
bool Sys_Angle_WrapDegrees(float angle_deg, float *wrapped_deg);

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
bool Sys_Angle_WrapRadians(float angle_rad, float *wrapped_rad);

/** @} */

#endif // LAYERED_EMBEDDED_C_SYSTEM_ANGLE_H
