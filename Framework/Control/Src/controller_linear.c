/**
 * @file controller_linear.c
 * @brief 线性控制律实现。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，完全静态分配。
 *          按“目标限幅 -> 线性前馈 + 摩擦符号偏置 -> 输出限幅 -> 单步斜率限幅 ->
 *          保持带死区 / 最小步长约束”流水线计算前馈输出。
 * @ingroup Framework_Control
 */

#include "controller_linear.h"
#include "sys_math.h"
#include <stddef.h>

/**
 * @brief 线性控制律虚表 update 回调：执行前馈映射、摩擦补偿与步长/死区整形。
 * @param[in,out] controller_h 线性控制律基类指针（实际类型为 controller_linear_t*）。
 * @param[in]     target       期望目标值（单位：m/s）。
 * @param[in]     measure      当前测量值（前馈开环不使用，仅占位）。
 * @param[in]     dt_s         控制周期（单位：s，前馈不使用，仅占位）。
 * @param[out]    out          输出控制量指针（单位：duty）。
 * @return 执行状态：输入/结果非有限返回 ERROR；触发任意限幅或最小步长抬升返回 SATURATED；否则 OK。
 */
static controller_status_t ctrl_linear_update(controller_base_t *controller_h, float target,
                                              float measure, float dt_s, float *out) {
  (void)measure;
  (void)dt_s;

  controller_linear_t *linear = (controller_linear_t *)controller_h;
  if (linear == NULL || out == NULL) {
    return CONTROLLER_STATUS_ERROR;
  }

  // 目标值非有限时立即清零历史输出与当前输出，防止斜坡状态被污染
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

  // 静摩擦符号补偿启用时，按线性项符号施加 ±b；目标为 0 时 SYS_SIGN(0)==0 自动消去偏置
  float raw_linear = target * linear->k;
  float bias = linear->defriction_enabled ? (linear->b * SYS_SIGN(raw_linear)) : linear->b;
  float result = raw_linear + bias;
  if (!SYS_ISFINITE(result)) {
    linear->last_out = 0.0f;
    *out = 0.0f;
    return CONTROLLER_STATUS_ERROR;
  }

  // 第一级限幅：将静态前馈输出钳位在 [min_out, max_out]
  float clamped_out = SYS_CLAMP(result, linear->min_out, linear->max_out);
  if (clamped_out != result) {
    saturated = true;
  }
  result = clamped_out;

  // 第二级限幅（斜率保护）：限制单周期相对 last_out 的最大跃变幅度
  if (linear->limit_step) {
    clamped_out =
        SYS_CLAMP(result, linear->last_out - linear->max_step, linear->last_out + linear->max_step);
    if (clamped_out != result) {
      saturated = true;
    }
    result = clamped_out;
  }

  // 保持带死区与最小步长整形：微小波动直接保持 last_out，超出死区但不足 min_step 则推至 min_step
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

/**
 * @brief 线性控制律虚表 reset 回调：清零上一周期输出缓存。
 * @param[in,out] controller_h 线性控制律基类指针。
 */
static void ctrl_linear_reset(controller_base_t *controller_h) {
  controller_linear_t *linear = (controller_linear_t *)controller_h;
  if (linear != NULL) {
    linear->last_out = 0.0f;
  }
}

/** @brief 线性控制律静态只读虚表。 */
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

  // 步长约束一致性校验：死区必须严格小于最大步长，未启用 max_step 时不允许配置 min_step
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
