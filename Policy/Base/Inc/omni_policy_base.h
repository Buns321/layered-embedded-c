//
// Created by Buns_ on 2026/10/20.
//

#ifndef LAYERED_EMBEDDED_C_OMNI_POLICY_BASE_H
#define LAYERED_EMBEDDED_C_OMNI_POLICY_BASE_H

typedef struct omni_policy_base omni_policy_base_t;

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

typedef struct omni_policy_base {
  const omni_policy_ops_t *ops;
  const char              *name;
} omni_policy_base_t;

void Omni_Policy_Base_Init(
  omni_policy_base_t *this,
  const char *name,
  const omni_policy_ops_t *ops
);
void Omni_Policy_Base_Solve(
  omni_policy_base_t *this,
  float v,
  float dir,
  float yaw,
  float omega,
  float out[4]
);

#endif //LAYERED_EMBEDDED_C_OMNI_POLICY_BASE_H
