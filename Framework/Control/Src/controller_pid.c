/**
 * @file controller_pid.c
 * @brief 位置式 PID 控制律实现。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，完全静态分配。
 *          实现测量微分（Derivative-on-Measurement）、可选一阶低通不完全微分、
 *          积分钳位与输出饱和方向冻结（Conditional Integration）抗饱和机制。
 * @ingroup Framework_Control
 */

#include "controller_pid.h"
#include "sys_math.h"
#include <stddef.h>

/**
 * @brief 清零 PID 运行期内部状态（保留增益与限幅配置）。
 * @param[in,out] pid PID 控制律实例指针（非 NULL）。
 */
static void ctrl_pid_clearstate(controller_pid_t *pid) {
  pid->integral = 0.0f;
  pid->filtered_derivative = 0.0f;
  pid->last_measure = 0.0f;
  pid->last_out = 0.0f;
  pid->has_last_measure = false;
}

/**
 * @brief PID 控制律虚表 update 回调：执行位置式 PID 单步解算。
 * @param[in,out] controller_h PID 控制律基类指针（实际类型为 controller_pid_t*）。
 * @param[in]     target       期望目标值（单位：m/s）。
 * @param[in]     measure      当前测量值（单位：m/s）。
 * @param[in]     dt_s         控制周期（单位：s，须 > 0）。
 * @param[out]    out          输出控制量指针（单位：duty）。
 * @return 执行状态：输入/计算异常返回 ERROR 并清零状态；触发限幅返回 SATURATED；否则 OK。
 */
static controller_status_t ctrl_pid_update(controller_base_t *controller_h, float target,
                                           float measure, float dt_s, float *out) {
  controller_pid_t *pid = (controller_pid_t *)controller_h;
  if (pid == NULL || out == NULL) {
    return CONTROLLER_STATUS_ERROR;
  }

  // 边界保护：任一输入非有限或 dt_s <= 0 时复位全部状态并清零输出，防止积分污染
  if (!SYS_ISFINITE(target) || !SYS_ISFINITE(measure) || !SYS_IS_POS_FINITE(dt_s)) {
    ctrl_pid_clearstate(pid);
    *out = 0.0f;
    return CONTROLLER_STATUS_ERROR;
  }

  bool saturated = false;
  if (pid->limit_target) {
    float clamped_target = SYS_CLAMP(target, pid->min_target, pid->max_target);
    if (clamped_target != target) {
      saturated = true;
    }
    target = clamped_target;
  }

  float error = target - measure;
  float proportional = pid->kp * error;

  // 测量微分（Derivative-on-Measurement）：对 -measure 求导而非对 error 求导，
  // 可彻底消除 target 阶跃跳变导致的微分冲击；首拍无历史测量值时微分项保持 0
  float derivative = 0.0f;
  if (pid->has_last_measure) {
    derivative = -(measure - pid->last_measure) / dt_s;
  }

  // 不完全微分（一阶低通）：alpha = tau / (tau + dt_s)，抑制编码器量化高频噪声放大
  float next_filtered_derivative = derivative;
  if (pid->derivative_tau_s > 0.0f) {
    float alpha = pid->derivative_tau_s / (pid->derivative_tau_s + dt_s);
    next_filtered_derivative = (alpha * pid->filtered_derivative) + ((1.0f - alpha) * derivative);
  }

  // 候选积分项先执行对称钳位（第一层抗积分饱和），防止极端长周期误差无限累积
  float candidate_integral = pid->integral + (pid->ki * error * dt_s);
  if (!SYS_ISFINITE(candidate_integral)) {
    ctrl_pid_clearstate(pid);
    *out = 0.0f;
    return CONTROLLER_STATUS_ERROR;
  }
  candidate_integral = SYS_CLAMP(candidate_integral, -pid->max_integral, pid->max_integral);

  float candidate_out = proportional + candidate_integral + (pid->kd * next_filtered_derivative);
  if (!SYS_ISFINITE(candidate_out)) {
    ctrl_pid_clearstate(pid);
    *out = 0.0f;
    return CONTROLLER_STATUS_ERROR;
  }

  // 条件积分抗饱和（Conditional Integration）：当总输出饱和时，若当前误差仍推动输出
  // 进一步越过饱和边界，则冻结积分器（不采纳 candidate_integral）；反向退饱和误差仍允许积分
  float clamped_out = SYS_CLAMP(candidate_out, pid->min_out, pid->max_out);
  if (clamped_out != candidate_out) {
    saturated = true;
    bool pushes_upper = (candidate_out > pid->max_out) && (error > 0.0f);
    bool pushes_lower = (candidate_out < pid->min_out) && (error < 0.0f);
    if (!pushes_upper && !pushes_lower) {
      pid->integral = candidate_integral;
    }
  } else {
    pid->integral = candidate_integral;
  }

  pid->filtered_derivative = next_filtered_derivative;
  pid->last_measure = measure;
  pid->last_out = clamped_out;
  pid->has_last_measure = true;

  *out = clamped_out;
  return saturated ? CONTROLLER_STATUS_SATURATED : CONTROLLER_STATUS_OK;
}

/**
 * @brief PID 控制律虚表 reset 回调：清零积分与微分历史状态。
 * @param[in,out] controller_h PID 控制律基类指针。
 */
static void ctrl_pid_reset(controller_base_t *controller_h) {
  controller_pid_t *pid = (controller_pid_t *)controller_h;
  if (pid != NULL) {
    ctrl_pid_clearstate(pid);
  }
}

/** @brief PID 控制律静态只读虚表。 */
static const controller_ops_t s_ctrl_pid_ops = {
    .update = ctrl_pid_update,
    .reset = ctrl_pid_reset,
};

bool Ctrl_PID_Init(controller_pid_t *pid, const char *name, const ctrl_pid_cfg_t *cfg) {
  if (pid == NULL || name == NULL || cfg == NULL) {
    return false;
  }
  if (!SYS_IS_NONNEG_FINITE(cfg->kp) || !SYS_IS_NONNEG_FINITE(cfg->ki) ||
      !SYS_IS_NONNEG_FINITE(cfg->kd) || !SYS_ISFINITE(cfg->max_target) ||
      !SYS_ISFINITE(cfg->min_target) || !SYS_IS_POS_FINITE(cfg->max_out) ||
      !SYS_ISFINITE(cfg->min_out) || cfg->min_out > 0.0f ||
      !SYS_IS_NONNEG_FINITE(cfg->max_integral) || !SYS_IS_NONNEG_FINITE(cfg->derivative_tau_s)) {
    return false;
  }

  // 解析目标限幅区间：全 0 关闭限幅，仅填正上限时自动对称生成负下限
  bool limit_target = true;
  float max_target = cfg->max_target;
  float min_target = cfg->min_target;
  if (max_target == 0.0f && min_target == 0.0f) {
    limit_target = false;
  } else if (min_target == 0.0f && max_target > 0.0f) {
    min_target = -max_target;
  } else if (max_target < min_target) {
    return false;
  }

  float min_out = (cfg->min_out == 0.0f) ? -cfg->max_out : cfg->min_out;
  float max_integral = (cfg->max_integral == 0.0f) ? cfg->max_out : cfg->max_integral;

  Ctrl_Base_Init(&pid->base, name, &s_ctrl_pid_ops);
  pid->kp = cfg->kp;
  pid->ki = cfg->ki;
  pid->kd = cfg->kd;
  pid->max_target = max_target;
  pid->min_target = min_target;
  pid->max_out = cfg->max_out;
  pid->min_out = min_out;
  pid->max_integral = max_integral;
  pid->derivative_tau_s = cfg->derivative_tau_s;
  pid->limit_target = limit_target;
  ctrl_pid_clearstate(pid);
  return true;
}
