/**
 * @file controller_pid.c
 * @brief 轮级 SISO 位置式 PID 控制律实现。
 * @ingroup controller
 */

#include "controller_pid.h"
#include "system_math.h"
#include <assert.h>

/**
 * @brief 清除 PID 积分、微分和上次采样状态。
 * @param controller_pid_h 已初始化的 PID 对象，不能为 NULL。
 * @pre 当前没有并发更新；配置参数和 ops 绑定已有效。
 * @post integral、filtered_derivative、last_measure、last_out 清零，has_last_measure 置为 false。
 * @note 不修改 PID 参数；不分配动态内存，调用后下一帧不产生微分冲击。
 */
static void ctrl_pid_clearstate(controller_pid_t *controller_pid_h) {
  assert(controller_pid_h);

  controller_pid_h->integral = 0.0f;
  controller_pid_h->filtered_derivative = 0.0f;
  controller_pid_h->last_measure = 0.0f;
  controller_pid_h->last_out = 0.0f;
  controller_pid_h->has_last_measure = false;
}

/**
 * @brief 计算位置式 PID 输出并处理限幅与微分滤波。
 * @param controller_h 已初始化的 PID 控制律基对象，不能为 NULL。
 * @param target 目标轮速，单位 m/s；必须有限，超出配置范围时夹紧并报告饱和。
 * @param measure 实测轮速，单位 m/s；必须有限，微分项基于相邻测量差值。
 * @param dt 距上次更新的时间步，单位 s；必须有限且大于 0。
 * @param out 调用方拥有的输出存储，不能为 NULL；写入 duty 结果。
 * @retval CONTROLLER_STATUS_OK 目标和输出均未触发限幅。
 * @retval CONTROLLER_STATUS_SATURATED 目标或候选输出触发限幅；条件积分可能冻结积分项。
 * @retval CONTROLLER_STATUS_ERROR target、measure 或最终输出不为有限值；全部运行时状态清零且 out 为 0。
 * @pre PID 已初始化；dt 来自真实控制周期，out 可写且不与内部状态重叠。
 * @post 成功时更新积分、滤波微分、上次测量和上次输出；饱和时按条件积分规则更新或保持积分，并更新其余运行时状态；ERROR 时保留参数但清空运行时状态。
 * @note 微分作用于测量值以避免目标突变冲击；同一实例只允许单写者调用。
 */
static controller_status_t ctrl_pid_update(
  controller_base_t *controller_h,
  float target,
  float measure,
  float dt,
  float *out
) {
  assert(controller_h && out);

  controller_pid_t *controller_pid_h = (controller_pid_t *)controller_h;

  if (
    !SYS_ISFINITE(target) ||
    !SYS_ISFINITE(measure)
  ) {
    ctrl_pid_clearstate(controller_pid_h);
    *out = 0.0f;
    return CONTROLLER_STATUS_ERROR;
  }

  bool saturated = false;

  float clamped_target = SYS_CLAMP(
    target,
    controller_pid_h->min_target,
    controller_pid_h->max_target
  );
  if (clamped_target != target) {
    saturated = true;
  }
  target = clamped_target;

  float error = target - measure;
  float proportional = controller_pid_h->kp * error;

  float derivative = 0.0f;
  if (controller_pid_h->has_last_measure) {
    derivative = -(measure - controller_pid_h->last_measure) / dt;
  }

  if (controller_pid_h->derivative_filter_tau > 0.0f) {
    float alpha =
      controller_pid_h->derivative_filter_tau /
      (controller_pid_h->derivative_filter_tau + dt);

    controller_pid_h->filtered_derivative =
      alpha * controller_pid_h->filtered_derivative +
      (1.0f - alpha) * derivative;
  } else {
    controller_pid_h->filtered_derivative = derivative;
  }

  float candidate_integral =
    controller_pid_h->integral +
    controller_pid_h->ki * error * dt;
  candidate_integral = SYS_CLAMP(
    candidate_integral,
    -controller_pid_h->max_integral,
    controller_pid_h->max_integral
  );

  float candidate_out =
    proportional +
    candidate_integral +
    controller_pid_h->kd * controller_pid_h->filtered_derivative;
  float clamped_out = SYS_CLAMP(
    candidate_out,
    controller_pid_h->min_out,
    controller_pid_h->max_out
  );

  bool output_saturated = clamped_out != candidate_out;
  if (output_saturated) {
    saturated = true;

    bool pushes_upper =
      candidate_out > controller_pid_h->max_out &&
      error > 0.0f;
    bool pushes_lower =
      candidate_out < controller_pid_h->min_out &&
      error < 0.0f;

    if (!pushes_upper && !pushes_lower) {
      controller_pid_h->integral = candidate_integral;
    }
  } else {
    controller_pid_h->integral = candidate_integral;
  }

  if (!SYS_ISFINITE(clamped_out)) {
    ctrl_pid_clearstate(controller_pid_h);
    *out = 0.0f;
    return CONTROLLER_STATUS_ERROR;
  }

  controller_pid_h->last_measure = measure;
  controller_pid_h->last_out = clamped_out;
  controller_pid_h->has_last_measure = true;

  *out = clamped_out;
  return saturated ? CONTROLLER_STATUS_SATURATED : CONTROLLER_STATUS_OK;
}

/**
 * @brief 重置 PID 内部状态。
 * @param controller_h 已初始化的 PID 控制律基对象，不能为 NULL。
 * @pre 当前没有并发更新；PID 参数无需重新初始化。
 * @post 积分、滤波微分、上次测量和上次输出清零，has_last_measure 置为 false。
 * @note 不修改增益、限幅或滤波时间常数；不分配动态内存。
 */
static void ctrl_pid_reset(controller_base_t *controller_h) {
  assert(controller_h);

  ctrl_pid_clearstate((controller_pid_t *)controller_h);
}

static const controller_ops_t controller_ops_pid = {
  .update = ctrl_pid_update,
  .reset  = ctrl_pid_reset,
};

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
) {
  assert(
    controller_h                         &&
    name                                 &&
    SYS_ISFINITE(kp)                     &&
    SYS_ISFINITE(ki)                     &&
    SYS_ISFINITE(kd)                     &&
    SYS_ISFINITE(max_target)             &&
    SYS_ISFINITE(min_target)             &&
    SYS_ISFINITE(max_out)                &&
    SYS_ISFINITE(min_out)                &&
    SYS_ISFINITE(max_integral)           &&
    SYS_ISFINITE(derivative_filter_tau)  &&
    kp >= 0.0f                           &&
    ki >= 0.0f                           &&
    kd >= 0.0f                           &&
    max_target >= min_target             &&
    max_out >= min_out                   &&
    max_out >= 0.0f                      &&
    min_out <= 0.0f                      &&
    max_integral >= 0.0f                 &&
    derivative_filter_tau >= 0.0f
  );

  Ctrl_Base_Init(&controller_h->controller_base, name, &controller_ops_pid);

  controller_h->kp = kp;
  controller_h->ki = ki;
  controller_h->kd = kd;
  controller_h->max_target = max_target;
  controller_h->min_target = min_target;
  controller_h->max_out = max_out;
  controller_h->min_out = min_out;
  controller_h->max_integral = max_integral;
  controller_h->derivative_filter_tau = derivative_filter_tau;

  ctrl_pid_clearstate(controller_h);
}
