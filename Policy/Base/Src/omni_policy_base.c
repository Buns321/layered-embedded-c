//
// 由 Buns_ 创建于 2026/10/20。
//

#include "omni_policy_base.h"
#include <assert.h>

void Omni_Policy_Base_Init(
  omni_policy_base_t *this,
  const char *name,
  const omni_policy_ops_t *ops
) {
  assert(this && name && ops && ops->solve);

  this->name = name;
  this->ops  = ops;
}

void Omni_Policy_Base_Solve(
  omni_policy_base_t *this,
  float v,
  float dir,
  float yaw,
  float omega,
  float out[4]
) {
  assert(this && this->ops && this->ops->solve && out);
  this->ops->solve(this, v, dir, yaw, omega, out);
}
