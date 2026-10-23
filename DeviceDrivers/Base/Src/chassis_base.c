//
// 由 Buns_ 创建于 2026/10/20。
//

#include "chassis_base.h"
#include <assert.h>

void Chassis_Base_Init(chassis_base_t *chassis_h, const char *name, const chassis_ops_t *ops) {
  assert(
    chassis_h        &&
    name        &&
    ops         &&
    ops->update &&
    ops->stop   &&
    ops->estop
  );

  chassis_h->name = name;
  chassis_h->ops  = ops;
}

void Chassis_Base_Update(chassis_base_t *chassis_h, uint32_t now_ms) {
  assert(chassis_h && chassis_h->ops && chassis_h->ops->update);
  chassis_h->ops->update(chassis_h, now_ms);
}

void Chassis_Base_Stop(chassis_base_t *chassis_h) {
  assert(chassis_h && chassis_h->ops && chassis_h->ops->stop);
  chassis_h->ops->stop(chassis_h);
}

void Chassis_Base_EStop(chassis_base_t *chassis_h) {
  assert(chassis_h && chassis_h->ops && chassis_h->ops->estop);
  chassis_h->ops->estop(chassis_h);
}
