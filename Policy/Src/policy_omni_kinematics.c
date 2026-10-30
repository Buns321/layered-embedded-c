//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file policy_omni_kinematics.c
 * @brief Omni 运动学策略实现。
 * @ingroup policy
 */

#include "policy_omni_kinematics.h"
#include "system_math.h"
#include <assert.h>
#include <math.h>

/**
 * @brief 按世界系方向、当前 yaw 与自转叠加解算四轮目标线速度。
 * @param policy_h 已初始化的运动学策略基对象，不能为 NULL。
 * @param v 世界系平动线速度，单位 m/s；必须有限。
 * @param dir 世界系行进方向，单位 rad，逆时针为正；必须有限。
 * @param yaw 当前机身朝向，单位 rad；必须有限。
 * @param omega 自转角速度，单位 rad/s，逆时针为正；必须有限。
 * @param out 调用方拥有的四元素输出数组，不能为 NULL；写入轮速 m/s。
 * @pre 策略已初始化，wheel_theta、wheel_sign 和 wheel_distance 配置有效。
 * @post 四个输出均被写入；任一输入非有限时四个输出全部清零并直接返回。
 * @note 平动项使用 cos(dir - (wheel_theta + yaw))，自转项使用
 *       omega*wheel_distance*wheel_sign；不分配动态内存，单写者调用。
 */
static void policy_omni_kinematics_solve(
  policy_omni_base_t *policy_h,
  float v,
  float dir,
  float yaw,
  float omega,
  float out[4]
) {
  assert(policy_h && out);

  policy_omni_kinematics_t *policy_omni_kinematics_h = (policy_omni_kinematics_t *)policy_h;

  if (
    !SYS_ISFINITE(v)     ||
    !SYS_ISFINITE(dir)   ||
    !SYS_ISFINITE(yaw)   ||
    !SYS_ISFINITE(omega)
  ) {
    for (uint8_t i = 0; i < 4; i++) {
      out[i] = 0.0f;
    }
    return;
  }

  for (uint8_t i = 0; i < 4; i++) {
    float alpha = policy_omni_kinematics_h->wheel_theta[i] + yaw;
    float trans = v * cosf(dir - alpha);
    float rotate = omega * policy_omni_kinematics_h->wheel_distance * policy_omni_kinematics_h->wheel_sign[i];

    out[i] = trans + rotate;
  }
}

static const policy_omni_ops_t policy_omni_ops = {
  .solve = policy_omni_kinematics_solve,
};

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
) {
  assert(
    policy_h            &&
    name            &&
    wheel_theta     &&
    wheel_sign      &&
    SYS_ISFINITE(wheel_distance) &&
    wheel_distance > 0.0f
  );

  for (uint8_t i = 0; i < 4; i++) {
    assert(SYS_ISFINITE(wheel_theta[i]));
    assert(SYS_ISFINITE(wheel_sign[i]));
  }

  Policy_Omni_Base_Init(&policy_h->base, name, &policy_omni_ops);

  policy_h->wheel_distance = wheel_distance;

  for (uint8_t i = 0; i < 4; i++) {
    policy_h->wheel_theta[i] = wheel_theta[i];
    policy_h->wheel_sign[i]  = wheel_sign[i];
  }
}
