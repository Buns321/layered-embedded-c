//
// 由 Buns_ 创建于 2026/10/17。
//

/**
 * @file controller_base.c
 * @brief Controller 抽象接口的通用转发实现。
 * @ingroup controller
 */

#include "controller_base.h"
#include "system_math.h"
#include <assert.h>

/** @brief 初始化控制律抽象层并绑定操作表。 */
void Ctrl_Base_Init(controller_base_t *controller_h, const char *name, const controller_ops_t *ops) {
  assert(controller_h && name && ops && ops->reset && ops->update);
  // 不能写成 assert(controller_h && name && controller_h->ops->reset && controller_h->ops->update);
  //                                 ^~~~~~~~~           ^~~~~~~~~
  // 断言对象应该是传入的 ops 而非 controller_h 句柄的 ops，此时 controller_h->ops 还没有被赋值，断言必报错
  controller_h->name = name;
  controller_h->ops  = ops;
}

/**
 * @brief 运行一次控制律。
 * @param controller_h 控制律对象。
 * @param target 参考值。
 * @param measure 实测值。
 * @param dt 控制周期（s），必须有限且大于 0。
 * @param out 输出指针，不能为空。
 * @retval CONTROLLER_STATUS_OK 正常。
 * @retval CONTROLLER_STATUS_SATURATED 输出饱和。
 * @retval CONTROLLER_STATUS_ERROR 输入/输出错误。
 */
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

/** @brief 复位控制律内部状态。 */
void Ctrl_Base_Reset(controller_base_t *controller_h) {
  assert(controller_h && controller_h->ops && controller_h->ops->reset);
  controller_h->ops->reset(controller_h);
}
