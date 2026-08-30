//
// Created by Buns_ on 2026/8/30.
//

#include "../Inc/encoder_base.h"
#include  <assert.h>

void Dev_Encoder_Base_Init(encoder_base_t *this, const char *name, const encoder_ops_t *ops) {
  assert(this && name && ops);
  this->ops = ops;
  this->name = name;
}

void Dev_Encoder_Base_SetZero(encoder_base_t *this) {
  assert(this);
  this->ops->set_zero(this);
}

int32_t Dev_Encoder_Base_GetPosition(encoder_base_t *this) {
  assert(this);
  return this->ops->get_position(this);
}
