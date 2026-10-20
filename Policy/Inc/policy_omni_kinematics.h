//
// Created by Buns_ on 2026/10/20.
//

/**
 * @file policy_omni_kinematics.h
 * @brief Omni 运动学策略：世界系平动 + 独立自转。
 * @addtogroup policy
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_POLICY_OMNI_KINEMATICS_H
#define LAYERED_EMBEDDED_C_POLICY_OMNI_KINEMATICS_H

#include "omni_policy_base.h"
#include <stdint.h>

/**
 * @brief Omni 运动学策略对象。
 * @note wheel_theta 单位 rad；wheel_sign 通常为 ±1；wheel_distance 单位 m。
 */
typedef struct policy_omni_kinematics {
  omni_policy_base_t base;

  float wheel_theta[4];
  float wheel_sign[4];
  float wheel_distance;
} policy_omni_kinematics_t;

/**
 * @brief 初始化 Omni 运动学策略。
 * @param this 对象。
 * @param name 名称。
 * @param wheel_distance 轮子到车体中心距离（m）。
 * @param wheel_theta 四轮安装角（rad）。
 * @param wheel_sign 四轮方向符号。
 * @note 解算使用当前 yaw；底盘应每个控制周期读取 IMU 后调用。
 */
void Policy_Omni_Kinematics_Init(
  policy_omni_kinematics_t *this,
  const char *name,
  float wheel_distance,
  const float wheel_theta[4],
  const float wheel_sign[4]
);

/** @} */

#endif //LAYERED_EMBEDDED_C_POLICY_OMNI_KINEMATICS_H
