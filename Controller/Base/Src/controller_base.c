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

/**
 * @brief 初始化控制律抽象层并绑定操作表。
 * @param controller_h 待初始化的控制律基对象存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；调用方须保证其生命周期覆盖控制律对象的使用期。
 * @param ops 非空操作表；update 和 reset 必须有效，且其生命周期覆盖控制律对象。
 * @pre controller_h 可写；name 和 ops 在对象存活期间保持有效。
 * @post controller_h->name 和 controller_h->ops 已绑定；不分配或释放资源。
 * @note 仅用于初始化阶段，不得与同一实例的更新或复位并发。
 */
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
 * @param controller_h 已初始化的控制律对象，不能为 NULL。
 * @param target 参考量；轮速环为 m/s，必须为有限值，具体范围由控制律配置限制。
 * @param measure 实测量；单位必须与 target 一致，必须为有限值。
 * @param dt 距上次更新的时间步，单位 s；必须有限且大于 0。
 * @param out 调用方拥有的输出存储，不能为 NULL；轮速环写入 duty。
 * @retval CONTROLLER_STATUS_OK 输出有效且未触及控制律限幅。
 * @retval CONTROLLER_STATUS_SATURATED 输出或参考量已触及控制律限幅。
 * @retval CONTROLLER_STATUS_ERROR 输入或输出无法保持有限值；具体实现会把 out 清零。
 * @pre controller_h 已绑定有效 update 回调；out 可写且不与控制器内部状态重叠。
 * @post 具体控制律内部状态已推进一次；ERROR 时 out 为 0，其他状态 out 为本次有效输出。
 * @note 同一实例只允许单写者调用；不分配动态内存，不在函数内部进入阻塞等待。
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

/**
 * @brief 复位控制律内部状态。
 * @param controller_h 已初始化的控制律对象，不能为 NULL。
 * @pre controller_h 已绑定有效 reset 回调，且当前没有并发更新。
 * @post 具体控制律清空积分、微分、上次测量和输出状态；配置参数与名称保持不变。
 * @note 同一实例只允许单写者调用；不分配动态内存，不改变 name/ops 绑定。
 */
void Ctrl_Base_Reset(controller_base_t *controller_h) {
  assert(controller_h && controller_h->ops && controller_h->ops->reset);
  controller_h->ops->reset(controller_h);
}
