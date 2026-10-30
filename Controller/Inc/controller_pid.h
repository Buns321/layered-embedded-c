/**
 * @file controller_pid.h
 * @brief 轮级 SISO 位置式 PID 控制律。
 * @addtogroup controller
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_CONTROLLER_PID_H
#define LAYERED_EMBEDDED_C_CONTROLLER_PID_H

#include "controller_base.h"
#include <stdbool.h>

/**
 * @brief PID 控制律对象。
 * @note 控制律使用位置式结构，微分项作用于测量值，避免目标突变造成微分冲击。
 * @note 积分项和输出项均有限幅；输出饱和且误差继续推向饱和区时保持上一周期
 *       积分值，形成条件积分抗饱和。
 * @note 对象由调用方静态持有，不涉及动态内存。
 */
typedef struct controller_pid_s {
  controller_base_t controller_base;

  float kp;                    // 比例增益 (duty / 被控量)
  float ki;                    // 积分增益 (duty / (被控量 * s))
  float kd;                    // 微分增益 (duty * s / 被控量)
  float max_target;            // 目标上限
  float min_target;            // 目标下限
  float max_out;               // 输出上限 (duty)
  float min_out;               // 输出下限 (duty)
  float max_integral;          // 积分项绝对值上限 (duty)
  float derivative_filter_tau; // 微分一阶低通时间常数 (s)，0 表示关闭

  float integral;              // 当前积分项 (duty)
  float filtered_derivative;   // 当前低通微分项 (duty)
  float last_measure;          // 上次有效测量值
  float last_out;              // 上次输出 (duty)
  bool  has_last_measure;      // last_measure 是否有效
} controller_pid_t;

/**
 * @brief 初始化 PID 控制律。
 * @param controller_h 待初始化的 PID 控制律存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；生命周期必须覆盖控制律对象。
 * @param kp 比例增益，单位 duty/被控量，必须有限且非负。
 * @param ki 积分增益，单位 duty/(被控量*s)，必须有限且非负。
 * @param kd 微分增益，单位 duty*s/被控量，必须有限且非负。
 * @param max_target 目标上限，单位与被控量一致，必须有限且不小于 min_target。
 * @param min_target 目标下限，单位与被控量一致，必须有限且不大于 max_target。
 * @param max_out 输出上限，单位 duty，必须有限且不小于 min_out、非负。
 * @param min_out 输出下限，单位 duty，必须有限且不大于 max_out、非正。
 * @param max_integral 积分项绝对值上限，单位 duty，必须有限且非负。
 * @param derivative_filter_tau 微分一阶低通时间常数，单位 s；0 表示关闭，非零时必须有限且大于 0。
 * @pre 所有参数必须有限并满足上述范围；Debug 构建用 assert 检查。
 * @post PID 参数和 ops 已绑定，积分、微分滤波及上次测量状态清零；不分配动态内存。
 * @note 仅允许初始化阶段单线程调用，不得与更新或复位并发。
 */
void Ctrl_PID_Init(
  controller_pid_t *controller_h,
  const char *name,
  float kp,
  float ki,
  float kd,
  float max_target,
  float min_target,
  float max_out,
  float min_out,
  float max_integral,
  float derivative_filter_tau
);

/** @} */

#endif //LAYERED_EMBEDDED_C_CONTROLLER_PID_H
