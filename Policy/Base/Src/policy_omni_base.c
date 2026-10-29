//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file policy_omni_base.c
 * @brief Omni 策略抽象接口实现。
 * @ingroup policy
 */

#include "policy_omni_base.h"
#include <assert.h>

/** @brief 初始化 Omni 策略抽象层并绑定操作表。 */
void Policy_Omni_Base_Init(
  policy_omni_base_t *policy_h,
  const char *name,
  const policy_omni_ops_t *ops
) {
  assert(policy_h && name && ops && ops->solve);

  policy_h->name = name;
  policy_h->ops  = ops;
}

/**
 * @brief 解算四个轮子的目标线速度。
 * @param policy_h 策略对象。
 * @param v 世界系平动速度（m/s）。
 * @param dir 世界系行进方向（rad，逆时针为正）。
 * @param yaw 当前机身朝向（rad）。
 * @param omega 自转角速度（rad/s，逆时针为正）。
 * @param out 四轮目标线速度（m/s）。
 */
void Policy_Omni_Base_Solve(
  policy_omni_base_t *policy_h,
  float v,
  float dir,
  float yaw,
  float omega,
  float out[4]
) {
  assert(policy_h && policy_h->ops && policy_h->ops->solve && out);
  policy_h->ops->solve(policy_h, v, dir, yaw, omega, out);
}
