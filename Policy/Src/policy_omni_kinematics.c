//
// 由 Buns_ 创建于 2026/10/20。
//

#include "policy_omni_kinematics.h"
#include "system_math.h"
#include <assert.h>
#include <math.h>

static void Policy_Omni_Kinematics_Solve(
  omni_policy_base_t *this,
  float v,
  float dir,
  float yaw,
  float omega,
  float out[4]
) {
  assert(this && out);

  policy_omni_kinematics_t *self = (policy_omni_kinematics_t *)this;

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
    float alpha = self->wheel_theta[i] + yaw;
    float trans = v * cosf(dir - alpha);
    float rotate = omega * self->wheel_distance * self->wheel_sign[i];

    out[i] = trans + rotate;
  }
}

static const omni_policy_ops_t omni_policy_ops = {
  .solve = Policy_Omni_Kinematics_Solve,
};

void Policy_Omni_Kinematics_Init(
  policy_omni_kinematics_t *this,
  const char *name,
  float wheel_distance,
  const float wheel_theta[4],
  const float wheel_sign[4]
) {
  assert(
    this            &&
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

  Omni_Policy_Base_Init(&this->base, name, &omni_policy_ops);

  this->wheel_distance = wheel_distance;

  for (uint8_t i = 0; i < 4; i++) {
    this->wheel_theta[i] = wheel_theta[i];
    this->wheel_sign[i]  = wheel_sign[i];
  }
}
