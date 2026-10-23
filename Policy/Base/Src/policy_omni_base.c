//
// 由 Buns_ 创建于 2026/10/20。
//

#include "policy_omni_base.h"
#include <assert.h>

void Policy_Omni_Base_Init(
  policy_omni_base_t *policy_h,
  const char *name,
  const policy_omni_ops_t *ops
) {
  assert(policy_h && name && ops && ops->solve);

  policy_h->name = name;
  policy_h->ops  = ops;
}

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
