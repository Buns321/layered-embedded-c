/**
 * @file controller_base.c
 * @brief 轮级 SISO 控制律抽象分发实现。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，完全静态分配。
 *          负责统一校验控制律句柄、虚表指针及时间步长 dt_s 的有效性，并将计算与
 *          复位请求安全分发至具体派生控制律实现。
 * @ingroup Framework_Control
 */

#include "controller_base.h"
#include "sys_math.h"
#include <stddef.h>

void Ctrl_Base_Init(controller_base_t *controller_h, const char *name,
                    const controller_ops_t *ops) {
  // 校验基类句柄、实例名称及虚表完整性，缺失任一必需回调时拒绝绑定
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
  // 边界保护：句柄/虚表无效或 dt_s <= 0 / NaN / Inf 时强制清零输出，防止积分或微分除零失控
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
