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

/**
 * @brief 初始化 Omni 策略抽象层并绑定操作表。
 * @param policy_h 待初始化的策略基对象存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；生命周期必须覆盖策略对象。
 * @param ops 非空操作表；solve 必须有效，且其生命周期覆盖策略对象。
 * @pre policy_h 可写；name 和 ops 在策略对象存活期间保持有效。
 * @post policy_h->name 与 policy_h->ops 已绑定；不分配或释放资源。
 * @note 仅在初始化阶段调用，不得与同一实例的 solve 并发。
 */
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
 * @param policy_h 已初始化的策略对象，不能为 NULL。
 * @param v 世界系平动线速度，单位 m/s；必须有限。
 * @param dir 世界系行进方向，单位 rad，逆时针为正；必须有限。
 * @param yaw 当前机身朝向，单位 rad；必须有限。
 * @param omega 自转角速度，单位 rad/s，逆时针为正；必须有限。
 * @param out 调用方拥有的四元素输出数组，不能为 NULL；写入各轮目标线速度 m/s。
 * @pre policy_h 已绑定有效 solve 回调；out 至少有 4 个可写元素。
 * @post 回调必须写满 out[0..3]；具体策略在输入非有限时采用其文档约定的安全输出。
 * @note 单写者调用；解算不使用动态内存，是否允许 ISR 由具体策略实现决定。
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
