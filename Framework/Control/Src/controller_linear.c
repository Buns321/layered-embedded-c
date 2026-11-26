/**
 * @file controller_linear.c
 * @brief 线性控制律实现。
 * @ingroup control
 */

#include "controller_linear.h"
#include "sys_math.h"
#include <stddef.h>

static controller_status_t ctrl_linear_update(controller_base_t *controller_h, float target,
                                              float measure, float dt_s, float *out) {
  (void)measure;
  (void)dt_s;

  controller_linear_t *linear = (controller_linear_t *)controller_h;
  if (linear == NULL || out == NULL) {
    return CONTROLLER_STATUS_ERROR;
  }

  if (!SYS_ISFINITE(target)) {
    linear->last_out = 0.0f;
    *out = 0.0f;
    return CONTROLLER_STATUS_ERROR;
  }

  bool saturated = false;
  if (linear->limit_target) {
    float clamped_target = SYS_CLAMP(target, linear->min_target, linear->max_target);
    if (clamped_target != target) {
      saturated = true;
    }
    target = clamped_target;
  }

  float raw_linear = target * linear->k;
  float bias = linear->defriction_enabled ? (linear->b * SYS_SIGN(raw_linear)) : linear->b;
  float result = raw_linear + bias;
  if (!SYS_ISFINITE(result)) {
    linear->last_out = 0.0f;
    *out = 0.0f;
    return CONTROLLER_STATUS_ERROR;
  }

  float clamped_out = SYS_CLAMP(result, linear->min_out, linear->max_out);
  if (clamped_out != result) {
    saturated = true;
  }
  result = clamped_out;

  if (linear->limit_step) {
    clamped_out =
        SYS_CLAMP(result, linear->last_out - linear->max_step, linear->last_out + linear->max_step);
    if (clamped_out != result) {
      saturated = true;
    }
    result = clamped_out;
  }

  if (linear->deadzone > 0.0f && result < (linear->last_out + linear->deadzone) &&
      result > (linear->last_out - linear->deadzone)) {
    result = linear->last_out;
  } else if (linear->min_step > 0.0f && result != linear->last_out &&
             result < (linear->last_out + linear->min_step) &&
             result > (linear->last_out - linear->min_step)) {
    result =
        linear->last_out + ((result > linear->last_out) ? linear->min_step : -linear->min_step);
    saturated = true;
  }

  if (!SYS_ISFINITE(result)) {
    linear->last_out = 0.0f;
    *out = 0.0f;
    return CONTROLLER_STATUS_ERROR;
  }

  linear->last_out = result;
  *out = result;
  return saturated ? CONTROLLER_STATUS_SATURATED : CONTROLLER_STATUS_OK;
}

static void ctrl_linear_reset(controller_base_t *controller_h) {
  controller_linear_t *linear = (controller_linear_t *)controller_h;
  if (linear != NULL) {
    linear->last_out = 0.0f;
  }
}

static const controller_ops_t s_ctrl_linear_ops = {
    .update = ctrl_linear_update,
    .reset = ctrl_linear_reset,
};

bool Ctrl_Linear_Init(controller_linear_t *linear, const char *name, const ctrl_linear_cfg_t *cfg) {
  if (linear == NULL || name == NULL || cfg == NULL) {
    return false;
  }
  if (!SYS_ISFINITE(cfg->k) || !SYS_ISFINITE(cfg->b) || !SYS_ISFINITE(cfg->max_target) ||
      !SYS_ISFINITE(cfg->min_target) || !SYS_IS_POS_FINITE(cfg->max_out) ||
      !SYS_ISFINITE(cfg->min_out) || cfg->min_out > 0.0f || !SYS_IS_NONNEG_FINITE(cfg->max_step) ||
      !SYS_IS_NONNEG_FINITE(cfg->min_step) || !SYS_IS_NONNEG_FINITE(cfg->deadzone)) {
    return false;
  }

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

  bool limit_step = (cfg->max_step > 0.0f);
  if (limit_step) {
    if (cfg->max_step < cfg->min_step || cfg->deadzone >= cfg->max_step) {
      return false;
    }
  } else if (cfg->min_step != 0.0f) {
    return false;
  }

  float min_out = (cfg->min_out == 0.0f) ? -cfg->max_out : cfg->min_out;

  Ctrl_Base_Init(&linear->base, name, &s_ctrl_linear_ops);
  linear->k = cfg->k;
  linear->b = cfg->b;
  linear->defriction_enabled = cfg->defriction_enabled;
  linear->max_target = max_target;
  linear->min_target = min_target;
  linear->max_out = cfg->max_out;
  linear->min_out = min_out;
  linear->max_step = cfg->max_step;
  linear->min_step = cfg->min_step;
  linear->deadzone = cfg->deadzone;
  linear->limit_target = limit_target;
  linear->limit_step = limit_step;
  linear->last_out = 0.0f;
  return true;
}
