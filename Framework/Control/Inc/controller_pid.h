/**
 * @file controller_pid.h
 * @brief 轮级 SISO 位置式 PID 控制律（测量微分、一阶微分低通、条件积分抗饱和）。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，完全静态分配。
 *          采用“对测量值求微分”以消除目标阶跃冲击，并支持可选一阶低通滤波与
 *          抗积分饱和（积分钳位 + 饱和方向冻结）。
 *
 * @addtogroup Framework_Control
 * @{
 */

#ifndef LAYERED_CAR_CONTROLLER_PID_H
#define LAYERED_CAR_CONTROLLER_PID_H

#include "controller_base.h"
#include <stdbool.h>

/**
 * @brief PID 控制律配置参数（单位：轮速环目标/测量 m/s，输出 duty，时间 s）。
 */
typedef struct ctrl_pid_cfg_s {
  float kp;               /**< 比例增益（单位：duty/(m/s)，须 >= 0）。 */
  float ki;               /**< 积分增益（单位：duty/m，须 >= 0）。 */
  float kd;               /**< 微分增益（单位：duty/(m/s^2)，须 >= 0）。 */
  float max_target;       /**< 目标上限（单位：m/s；0 且 min_target==0 表示不限制目标）。 */
  float min_target;       /**< 目标下限（单位：m/s；0 且 max_target>0 时默认取 -max_target）。 */
  float max_out;          /**< 输出上限（单位：duty，必填 > 0）。 */
  float min_out;          /**< 输出下限（单位：duty，<= 0；0 时默认取 -max_out）。 */
  float max_integral;     /**< 积分项绝对值限幅（单位：duty，>= 0；0 时默认等于 max_out）。 */
  float derivative_tau_s; /**< 微分一阶低通时间常数（单位：s，>= 0；0 表示关闭微分低通）。 */
} ctrl_pid_cfg_t;

/**
 * @brief PID 控制律静态实例对象（base 必须为首成员，禁止按值拷贝）。
 */
typedef struct controller_pid_s {
  controller_base_t base;    /**< 多态基类（必须为首成员）。 */
  float kp;                  /**< 比例增益（单位：duty/(m/s)）。 */
  float ki;                  /**< 积分增益（单位：duty/m）。 */
  float kd;                  /**< 微分增益（单位：duty/(m/s^2)）。 */
  float max_target;          /**< 生效的目标上限（单位：m/s）。 */
  float min_target;          /**< 生效的目标下限（单位：m/s）。 */
  float max_out;             /**< 生效的输出上限（单位：duty）。 */
  float min_out;             /**< 生效的输出下限（单位：duty）。 */
  float max_integral;        /**< 生效的积分项绝对值限幅（单位：duty）。 */
  float derivative_tau_s;    /**< 微分一阶低通时间常数（单位：s）。 */
  bool limit_target;         /**< 是否启用目标输入限幅。 */
  float integral;            /**< 当前积分累计值（单位：duty）。 */
  float filtered_derivative; /**< 经一阶低通滤波后的测量微分值（单位：m/s^2）。 */
  float last_measure;        /**< 上一周期测量值（单位：m/s）。 */
  float last_out;            /**< 上一周期限幅后输出值（单位：duty）。 */
  bool has_last_measure;     /**< 是否已记录有效历史测量值（首拍无微分冲击）。 */
} controller_pid_t;

/**
 * @brief 校验配置并初始化 PID 控制律；失败时返回 false 且不修改 pid。
 * @param[out] pid  待初始化的 PID 控制律实例指针（非 NULL）。
 * @param[in]  name 控制律名称字符串（非 NULL）。
 * @param[in]  cfg  PID 配置参数结构体指针（非 NULL）。
 * @return 初始化是否成功。
 * @retval true  配置合法，实例已初始化并清零内部状态。
 * @retval false 参数为空、增益为负或限幅范围冲突（pid 保持原状）。
 */
bool Ctrl_PID_Init(controller_pid_t *pid, const char *name, const ctrl_pid_cfg_t *cfg);

#endif /* LAYERED_CAR_CONTROLLER_PID_H */

/** @} */
