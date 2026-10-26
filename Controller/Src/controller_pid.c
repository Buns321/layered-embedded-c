/**
 * @file controller_pid.c
 * @brief 轮级 SISO 位置式 PID 控制律实现。
 * @ingroup controller
 */

#include "controller_pid.h"
#include "system_math.h"
#include <assert.h>

static void Ctrl_PID_ClearState(controller_pid_t *controller_pid_h) {
  assert(controller_pid_h);

  controller_pid_h->integral = 0.0f;
  controller_pid_h->filtered_derivative = 0.0f;
  controller_pid_h->last_measure = 0.0f;
  controller_pid_h->last_out = 0.0f;
  controller_pid_h->has_last_measure = false;
}

static controller_status_t Ctrl_PID_Update(
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
    Ctrl_PID_ClearState(controller_pid_h);
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
    Ctrl_PID_ClearState(controller_pid_h);
    *out = 0.0f;
    return CONTROLLER_STATUS_ERROR;
  }

  controller_pid_h->last_measure = measure;
  controller_pid_h->last_out = clamped_out;
  controller_pid_h->has_last_measure = true;

  *out = clamped_out;
  return saturated ? CONTROLLER_STATUS_SATURATED : CONTROLLER_STATUS_OK;
}

static void Ctrl_PID_Reset(controller_base_t *controller_h) {
  assert(controller_h);

  Ctrl_PID_ClearState((controller_pid_t *)controller_h);
}

static const controller_ops_t controller_ops_pid = {
  .update = Ctrl_PID_Update,
  .reset  = Ctrl_PID_Reset,
};

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

  Ctrl_PID_ClearState(controller_h);
}
