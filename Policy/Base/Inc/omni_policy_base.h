//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file omni_policy_base.h
 * @brief Omni 底盘的运动策略接口。
 * @addtogroup policy
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_OMNI_POLICY_BASE_H
#define LAYERED_EMBEDDED_C_OMNI_POLICY_BASE_H

typedef struct omni_policy_base omni_policy_base_t;

/**
 * @brief Omni 策略操作表。
 */
typedef struct omni_policy_ops {
  void (*solve)(
    omni_policy_base_t *this,
    float v,
    float dir,
    float yaw,
    float omega,
    float out[4]
  );
} omni_policy_ops_t;

/**
 * @brief Omni 策略抽象对象。
 */
typedef struct omni_policy_base {
  const omni_policy_ops_t *ops;
  const char              *name;
} omni_policy_base_t;

/** @brief 初始化 Omni 策略抽象层并绑定操作表。 */
void Omni_Policy_Base_Init(
  omni_policy_base_t *this,
  const char *name,
  const omni_policy_ops_t *ops
);
/**
 * @brief 解算四个轮子的目标线速度。
 * @param this 策略对象。
 * @param v 世界系平动速度（m/s）。
 * @param dir 世界系行进方向（rad，逆时针为正）。
 * @param yaw 当前机身朝向（rad）。
 * @param omega 自转角速度（rad/s，逆时针为正）。
 * @param out 四轮目标线速度（m/s）。
 */
void Omni_Policy_Base_Solve(
  omni_policy_base_t *this,
  float v,
  float dir,
  float yaw,
  float omega,
  float out[4]
);

/** @} */

#endif //LAYERED_EMBEDDED_C_OMNI_POLICY_BASE_H
