//
// 由 Buns_ 创建于 2026/10/17。
//

#include "controller_base.h"
#include "system_math.h"
#include <assert.h>

void Ctrl_Base_Init(controller_base_t *this, const char *name, const controller_ops_t *ops) {
  assert(this && name && ops && ops->reset && ops->update);
  // 不能写成 assert(this && name && this->ops->reset && this->ops->update);
  //                                 ^~~~~~~~~           ^~~~~~~~~
  // 断言对象应该是传入的 ops 而非 this 句柄的 ops，此时 this->ops 还没有被赋值，断言必报错
  this->name = name;
  this->ops  = ops;
}

controller_status_t Ctrl_Base_Update(
  controller_base_t *this,
  float target,
  float measure,
  float dt,
  float *out
) {
  assert(this && this->ops && this->ops->update && out);
  assert(SYS_ISFINITE(dt) && dt > 0.0f);
  return this->ops->update(this, target, measure, dt, out);
}

void Ctrl_Base_Reset(controller_base_t *this) {
  assert(this && this->ops && this->ops->reset);
  this->ops->reset(this);
}
