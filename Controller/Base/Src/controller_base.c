//
// Created by Buns_ on 2026/10/17.
//

#include "controller_base.h"
#include <assert.h>

void Ctrl_Base_Init(controller_base_t *this, const char *name, const controller_ops_t *ops) {
  assert(this && name && ops && ops->reset && ops->update);
  // 不能写成 assert(this && name && this->ops->reset && this->ops->update);
  //                                 ^~~~~~~~~           ^~~~~~~~~
  // 断言对象应该是传入的 ops 而非 this 句柄的 ops，此时 this->ops 还没有被赋值，断言必报错
  this->name = name;
  this->ops  = ops;
}

float Ctrl_Base_Update(controller_base_t *this, float target, float measure) {
  assert(this && this->ops && this->ops->update);
  return this->ops->update(this, target, measure);
}

void Ctrl_Base_Reset(controller_base_t *this) {
  assert(this && this->ops && this->ops->reset);
  this->ops->reset(this);
}
