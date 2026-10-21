//
// Created by Buns_ on 2026/10/20.
//

#include "chassis_base.h"
#include <assert.h>

void Chassis_Base_Init(chassis_base_t *this, const char *name, const chassis_ops_t *ops) {
  assert(
    this        &&
    name        &&
    ops         &&
    ops->update &&
    ops->stop   &&
    ops->estop
  );

  this->name = name;
  this->ops  = ops;
}

void Chassis_Base_Update(chassis_base_t *this, uint32_t now_ms) {
  assert(this && this->ops && this->ops->update);
  this->ops->update(this, now_ms);
}

void Chassis_Base_Stop(chassis_base_t *this) {
  assert(this && this->ops && this->ops->stop);
  this->ops->stop(this);
}

void Chassis_Base_EStop(chassis_base_t *this) {
  assert(this && this->ops && this->ops->estop);
  this->ops->estop(this);
}
