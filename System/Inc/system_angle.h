/**
 * @file system_angle.h
 * @brief 有限角度到规范周期的归一化。
 * @addtogroup system
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_ANGLE_H
#define LAYERED_EMBEDDED_C_SYSTEM_ANGLE_H

#include <stdbool.h>

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

#endif //LAYERED_EMBEDDED_C_SYSTEM_ANGLE_H
