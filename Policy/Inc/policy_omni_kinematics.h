//
// Created by Buns_ on 2026/10/20.
//

#ifndef LAYERED_EMBEDDED_C_POLICY_OMNI_KINEMATICS_H
#define LAYERED_EMBEDDED_C_POLICY_OMNI_KINEMATICS_H

#include "omni_policy_base.h"
#include <stdint.h>

typedef struct policy_omni_kinematics {
  omni_policy_base_t base;

  float wheel_theta[4];
  float wheel_sign[4];
  float wheel_distance;
} policy_omni_kinematics_t;

void Policy_Omni_Kinematics_Init(
  policy_omni_kinematics_t *this,
  const char *name,
  float wheel_distance,
  const float wheel_theta[4],
  const float wheel_sign[4]
);

#endif //LAYERED_EMBEDDED_C_POLICY_OMNI_KINEMATICS_H
