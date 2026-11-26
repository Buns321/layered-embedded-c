/**
 * @file controller_base.c
 * @brief 轮级 SISO 控制律抽象分发实现。
 * @ingroup control
 */

#include "controller_base.h"
#include "sys_math.h"
#include <stddef.h>

void Ctrl_Base_Init(controller_base_t *controller_h, const char *name,
                    const controller_ops_t *ops) {
  if (controller_h == NULL || name == NULL || ops == NULL || ops->update == NULL ||
      ops->reset == NULL) {
    return;
  }
  controller_h->name = name;
  controller_h->ops = ops;
}

controller_status_t Ctrl_Base_Update(controller_base_t *controller_h, float target, float measure,
                                     float dt_s, float *out) {
  if (out == NULL) {
    return CONTROLLER_STATUS_ERROR;
  }
  if (controller_h == NULL || controller_h->ops == NULL || controller_h->ops->update == NULL ||
      !SYS_IS_POS_FINITE(dt_s)) {
    *out = 0.0f;
    return CONTROLLER_STATUS_ERROR;
  }
  return controller_h->ops->update(controller_h, target, measure, dt_s, out);
}

void Ctrl_Base_Reset(controller_base_t *controller_h) {
  if (controller_h == NULL || controller_h->ops == NULL || controller_h->ops->reset == NULL) {
    return;
  }
  controller_h->ops->reset(controller_h);
}
