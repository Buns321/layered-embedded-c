//
// Created by Buns_ on 2026/8/30.
//

#include "encoder_base.h"
#include  <assert.h>

void Dev_Encoder_Base_Init(encoder_base_t *this, const char *name, const encoder_ops_t *ops) {
  assert(this && name && ops);
  this->ops = ops;
  this->name = name;
}

void Dev_Encoder_Base_SetZero(encoder_base_t *this) {
  assert(this && this->ops && this->ops->set_zero);
  this->ops->set_zero(this);
}

int32_t Dev_Encoder_Base_GetPosition(encoder_base_t *this) {
  assert(this && this->ops && this->ops->get_position);
  return this->ops->get_position(this);
}

int32_t Dev_Encoder_Base_GetDelta(encoder_base_t *this){
  assert(this && this->ops && this->ops->get_delta);
  return this->ops->get_delta(this);
}

void Dev_Encoder_Base_Resync(encoder_base_t *this){
  assert(this && this->ops && this->ops->resync);
  this->ops->resync(this);
}
