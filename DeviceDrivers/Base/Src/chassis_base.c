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

/**
 * @brief 绑定底盘名称与生命周期操作表。
 * @param chassis_h 调用方持有的底盘基类存储；不可为 NULL，所有权不转移。
 * @param name 只读调试名称，生命周期必须覆盖底盘对象；不可为 NULL。
 * @param ops 静态操作表；不可为 NULL，update、stop、estop 必须全部可用。
 * @pre chassis_h 尚未并发发布，name 与 ops 在对象使用期间保持有效。
 * @post 只在基类中保存 name 与 ops，不初始化具体底盘，也不访问硬件。
 * @note 仅限初始化阶段调用；不加锁且不分配内存，重复调用会覆盖已有绑定。
 */
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
 * @brief 委托具体底盘执行一次解算与目标下发。
 * @param chassis_h 已初始化的底盘句柄；不可为 NULL，所有权不转移。
 * @param now_ms 单调毫秒时间戳；须与 IMU 时间戳使用同一时基。
 * @pre 已通过 Chassis_Base_Init 绑定非空 update 操作。
 * @post 具体实现可更新目标、制动或执行器输出，本层不额外修改基类状态。
 * @note 通常在控制任务调用；基类不加锁，同一句柄的生命周期操作须由调用方串行化。
 * @warning 函数无返回值；具体实现失败只能通过底盘或电机状态观察，前置条件违反由 assert 捕获。
 */
void Chassis_Base_Update(chassis_base_t *chassis_h, uint32_t now_ms) {
  assert(chassis_h && chassis_h->ops && chassis_h->ops->update);
  chassis_h->ops->update(chassis_h, now_ms);
}

/**
 * @brief 委托具体底盘执行常规停机。
 * @param chassis_h 已初始化的底盘句柄；不可为 NULL，所有权不转移。
 * @pre 已通过 Chassis_Base_Init 绑定非空 stop 操作。
 * @post 具体实现通常清零目标并请求电机制动；制动保持时间由具体实现决定。
 * @note 通常在控制或监控任务调用；不加锁，须与 Update、EStop 串行化。
 * @warning 无返回值且不传播具体实现错误；重新闭环运行必须由上层显式执行。
 */
void Chassis_Base_Stop(chassis_base_t *chassis_h) {
  assert(chassis_h && chassis_h->ops && chassis_h->ops->stop);
  chassis_h->ops->stop(chassis_h);
}

/**
 * @brief 委托具体底盘执行紧急停机。
 * @param chassis_h 已初始化的底盘句柄；不可为 NULL，所有权不转移。
 * @pre 已通过 Chassis_Base_Init 绑定非空 estop 操作。
 * @post 具体实现通常立即禁用执行器输出，恢复运行须重新使能。
 * @note 不加锁；若具体实现需要抢占普通控制路径，须由调用方提供并发策略。
 * @warning 无返回值且不传播具体实现错误，安全关键路径仍需检查电机实际状态。
 */
void Chassis_Base_EStop(chassis_base_t *chassis_h) {
  assert(chassis_h && chassis_h->ops && chassis_h->ops->estop);
  chassis_h->ops->estop(chassis_h);
}
