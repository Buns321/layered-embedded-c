//
// Created by Buns_ on 2026/10/20.
//

#ifndef LAYERED_EMBEDDED_C_CHASSIS_OMNI_H
#define LAYERED_EMBEDDED_C_CHASSIS_OMNI_H

#include "chassis_base.h"
#include "motor_base.h"
#include "imu_base.h"
#include "omni_policy_base.h"
#include <stdint.h>

#define CHASSIS_OMNI_WHEEL_COUNT 4u

typedef struct chassis_omni {
  chassis_base_t      base;
  motor_base_t       *wheel[CHASSIS_OMNI_WHEEL_COUNT];
  imu_base_t         *imu;
  omni_policy_base_t *policy;

  float v;
  float dir;
  float omega;
} chassis_omni_t;

void Chassis_Omni_Init(
  chassis_omni_t     *this,
  const char         *name,
  motor_base_t *const wheel[CHASSIS_OMNI_WHEEL_COUNT],
  imu_base_t         *imu,
  omni_policy_base_t *policy
);
void Chassis_Omni_SetMotion(chassis_omni_t *this, float v, float dir, float omega);

#endif //LAYERED_EMBEDDED_C_CHASSIS_OMNI_H
