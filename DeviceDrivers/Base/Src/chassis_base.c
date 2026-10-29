//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file chassis_base.c
 * @brief 底盘生命周期抽象接口实现。
 * @ingroup drivers_base
 */

#include "chassis_base.h"
#include <assert.h>

/** @brief 初始化底盘抽象层并绑定操作表。 */
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

/**
 * @brief 运行一次底盘解算与目标下发。
 * @param chassis_h 底盘对象。
 * @param now_ms 当前单调毫秒时间戳，用于 IMU 数据新鲜度判断。
 */
void Chassis_Base_Update(chassis_base_t *chassis_h, uint32_t now_ms) {
  assert(chassis_h && chassis_h->ops && chassis_h->ops->update);
  chassis_h->ops->update(chassis_h, now_ms);
}

/**
 * @brief 常规停机：清零目标并制动。
 * @note 制动状态保持到电机被显式重新使能。
 */
void Chassis_Base_Stop(chassis_base_t *chassis_h) {
  assert(chassis_h && chassis_h->ops && chassis_h->ops->stop);
  chassis_h->ops->stop(chassis_h);
}

/** @brief 紧急停机：禁用执行器输出。 */
void Chassis_Base_EStop(chassis_base_t *chassis_h) {
  assert(chassis_h && chassis_h->ops && chassis_h->ops->estop);
  chassis_h->ops->estop(chassis_h);
}
