/**
 * @file chassis_base.c
 * @brief 底盘生命周期多态分发实现。
 * @details 本文件属于 Framework/Chassis 层，提供 chassis_base_t 虚表绑定与安全转发，
 *          所有分发入口均校验基类句柄与虚表函数指针非 NULL，避免空指针解引用。
 * @ingroup Framework_Chassis
 */

#include "chassis_base.h"
#include <stddef.h>

void Chassis_Base_Init(chassis_base_t *chassis_h, const char *name, const chassis_ops_t *ops) {
  // 必须确保基类指针、名称与 4 个虚表槽位全部有效，防止运行期出现半初始化虚表
  if (chassis_h == NULL || name == NULL || ops == NULL || ops->set_motion == NULL ||
      ops->update == NULL || ops->stop == NULL || ops->estop == NULL) {
    return;
  }
  chassis_h->name = name;
  chassis_h->ops = ops;
}

void Chassis_Base_SetMotion(chassis_base_t *chassis_h, const chassis_twist_t *twist) {
  if (chassis_h == NULL || chassis_h->ops == NULL || chassis_h->ops->set_motion == NULL ||
      twist == NULL) {
    return;
  }
  chassis_h->ops->set_motion(chassis_h, twist);
}

void Chassis_Base_Update(chassis_base_t *chassis_h, uint32_t now_ms) {
  if (chassis_h == NULL || chassis_h->ops == NULL || chassis_h->ops->update == NULL) {
    return;
  }
  chassis_h->ops->update(chassis_h, now_ms);
}

void Chassis_Base_Stop(chassis_base_t *chassis_h) {
  if (chassis_h == NULL || chassis_h->ops == NULL || chassis_h->ops->stop == NULL) {
    return;
  }
  chassis_h->ops->stop(chassis_h);
}

void Chassis_Base_EStop(chassis_base_t *chassis_h) {
  if (chassis_h == NULL || chassis_h->ops == NULL || chassis_h->ops->estop == NULL) {
    return;
  }
  chassis_h->ops->estop(chassis_h);
}
