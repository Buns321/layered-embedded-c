//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file policy_omni_kinematics.h
 * @brief Omni 运动学策略：世界系平动 + 独立自转。
 * @addtogroup policy
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_POLICY_OMNI_KINEMATICS_H
#define LAYERED_EMBEDDED_C_POLICY_OMNI_KINEMATICS_H

#include "policy_omni_base.h"
#include <stdint.h>

/**
 * @brief Omni 运动学策略对象。
 * @note wheel_theta 单位 rad；wheel_sign 通常为 ±1；wheel_distance 单位 m。
 */
typedef struct policy_omni_kinematics_s {
  policy_omni_base_t base;

  float wheel_theta[4];
  float wheel_sign[4];
  float wheel_distance;
} policy_omni_kinematics_t;

/**
 * @brief 初始化 Omni 运动学策略。
 * @param policy_h 待初始化的运动学策略存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；生命周期必须覆盖策略对象。
 * @param wheel_distance 轮子到车体中心距离，单位 m；必须有限且大于 0。
 * @param wheel_theta 四轮安装角数组，单位 rad；四项必须有限。
 * @param wheel_sign 四轮方向符号数组，通常取 ±1；四项必须有限，本函数只检查有限性。
 * @pre policy_h 可写；name 的生命周期覆盖策略对象；wheel_theta 和 wheel_sign 只需在调用期间可读。
 * @post 基对象名称与 ops 已绑定，wheel_distance、wheel_theta 和 wheel_sign 已复制到对象；不保留输入数组指针。
 * @note 仅允许初始化阶段单线程调用；不分配动态内存。
 */
void Policy_Omni_Kinematics_Init(
  policy_omni_kinematics_t *policy_h,
  const char *name,
  float wheel_distance,
  const float wheel_theta[4],
  const float wheel_sign[4]
);

/** @} */

#endif //LAYERED_EMBEDDED_C_POLICY_OMNI_KINEMATICS_H
