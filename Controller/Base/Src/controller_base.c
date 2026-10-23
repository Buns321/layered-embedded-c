//
// 由 Buns_ 创建于 2026/10/17。
//

#include "controller_base.h"
#include "system_math.h"
#include <assert.h>

void Ctrl_Base_Init(controller_base_t *controller_h, const char *name, const controller_ops_t *ops) {
  assert(controller_h && name && ops && ops->reset && ops->update);
  // 不能写成 assert(controller_h && name && controller_h->ops->reset && controller_h->ops->update);
  //                                 ^~~~~~~~~           ^~~~~~~~~
  // 断言对象应该是传入的 ops 而非 controller_h 句柄的 ops，此时 controller_h->ops 还没有被赋值，断言必报错
  controller_h->name = name;
  controller_h->ops  = ops;
}

controller_status_t Ctrl_Base_Update(
  controller_base_t *controller_h,
  float target,
  float measure,
  float dt,
  float *out
) {
  assert(controller_h && controller_h->ops && controller_h->ops->update && out);
  assert(SYS_ISFINITE(dt) && dt > 0.0f);
  return controller_h->ops->update(controller_h, target, measure, dt, out);
}

void Ctrl_Base_Reset(controller_base_t *controller_h) {
  assert(controller_h && controller_h->ops && controller_h->ops->reset);
  controller_h->ops->reset(controller_h);
}
