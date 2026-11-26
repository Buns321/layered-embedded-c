/**
 * @file controller_pid.h
 * @brief 轮级 SISO 位置式 PID 控制律（测量微分、一阶微分低通、条件积分抗饱和）。
 * @addtogroup control
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
  float kp;
  float ki;
  float kd;
  float max_target;       /* 0 且 min_target==0 表示不限制目标 */
  float min_target;       /* min_target==0 && max_target>0 时默认取 -max_target */
  float max_out;          /* 必填 > 0 */
  float min_out;          /* 0 时默认取 -max_out */
  float max_integral;     /* 0 时默认等于 max_out */
  float derivative_tau_s; /* 0 = 关闭微分低通，单位 s */
} ctrl_pid_cfg_t;

/**
 * @brief PID 控制律静态实例对象（base 必须为首成员，禁止按值拷贝）。
 */
typedef struct controller_pid_s {
  controller_base_t base;
  float kp;
  float ki;
  float kd;
  float max_target;
  float min_target;
  float max_out;
  float min_out;
  float max_integral;
  float derivative_tau_s;
  bool limit_target;
  float integral;
  float filtered_derivative;
  float last_measure;
  float last_out;
  bool has_last_measure;
} controller_pid_t;

/**
 * @brief 校验配置并初始化 PID 控制律；失败时返回 false 且不修改 pid。
 */
bool Ctrl_PID_Init(controller_pid_t *pid, const char *name, const ctrl_pid_cfg_t *cfg);

/** @} */

#endif /* LAYERED_CAR_CONTROLLER_PID_H */
