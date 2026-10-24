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

static void Policy_Omni_Kinematics_Solve(
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
  .solve = Policy_Omni_Kinematics_Solve,
};

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
