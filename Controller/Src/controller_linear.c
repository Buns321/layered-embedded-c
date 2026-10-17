//
// Created by Buns_ on 2026/10/17.
//

#include "../Inc/controller_linear.h"
#include "../../System/Inc/system_math.h"
#include <assert.h>

static float Ctrl_Linear_Update(controller_base_t *this, float target, float measure) {
  assert(this && SYS_ISFINITE(target) && SYS_ISFINITE(measure));
  (void) measure;
  controller_linear_t *self = (controller_linear_t *) this;

  // 非有限输入保护
  if (!SYS_ISFINITE(target)) {
    self->last_out = 0.0f;
    return 0.0f;
  }

  // 输入限幅
  target = SYS_CLAMP(target, self->min_target, self->max_target);

  float out = target * self->k + self->b * (self->is_defriction_enabled ? SYS_SIGN(target * self->k) : 1.0f);

  // 绝对输出最大&最小限幅
  out = SYS_CLAMP(out, self->min_out, self->max_out);

  // 相对输出最大限幅
  out = SYS_CLAMP(
    out,
    self->last_out - self->max_step,
    self->last_out + self->max_step
  );

  /* |-----------------|----------|----------|
   * | 死区 \ 最小步长 | FALSE    | TRUE     |
   * |-----------------|----------|----------|
   * | FALSE           | out      | min_step |
   * | TRUE            | last_out | last_out |
   * |-----------------|----------|----------|
   */

  // 输出死区
  if (
    out < self->last_out + self->deadzone &&
    out > self->last_out - self->deadzone
  ) out = self->last_out;

  // 相对输出最小限幅
  else if (
    out != self->last_out &&
    out < self->last_out + self->min_step &&
    out > self->last_out - self->min_step
  ) out = self->last_out + (out > self->last_out ? +self->min_step : -self->min_step);

  // 非有限输出保护
  if (!SYS_ISFINITE(out)) {
    self->last_out = 0.0f;
    return 0.0f;
  }

  // 更新 last_out
  self->last_out = out;

  return out;
}

static void Ctrl_Linear_Reset(controller_base_t *this) {
  assert(this);
  controller_linear_t *self = (controller_linear_t *) this;
  self->last_out = 0;
}

static const controller_ops_t controller_ops_linear = {
  .update = Ctrl_Linear_Update,
  .reset  = Ctrl_Linear_Reset,
};

void Ctrl_Linear_Init(
  controller_linear_t *controller_linear,
  const char *name,
  float k,                     // 斜率
  float b,                     // 截距
  bool  is_defriction_enabled, // 是否启用摩擦补偿
  float max_target,            // 最大输入限幅 (m/s)
  float min_target,            // 最小输入限幅 (m/s)
  float max_out,               // 最大输出限幅 (duty)
  float min_out,               // 最小输出限幅 (duty)
  float max_step,              // 最大步长限幅 (duty)
  float min_step,              // 最小步长限幅 (duty)
  float deadzone               // 死区         (duty)
) {
  assert(
    controller_linear != NULL       &&
    name              != NULL       &&
    SYS_ISFINITE(k)                 &&
    SYS_ISFINITE(b)                 &&
    SYS_ISFINITE(max_target)        &&
    SYS_ISFINITE(min_target)        &&
    SYS_ISFINITE(max_out)           &&
    SYS_ISFINITE(min_out)           &&
    SYS_ISFINITE(max_step)          &&
    SYS_ISFINITE(min_step)          &&
    SYS_ISFINITE(deadzone)          &&
    max_target        >= min_target &&
    max_out           >= min_out    &&
    max_out           >= 0          &&
    min_out           <= 0          &&
    max_step          >= min_step   &&
    max_step          >= 0          &&
    min_step          >= 0          &&
    deadzone          >= 0          &&
    deadzone          <  max_step
  );

  // TODO 也许等100年我把 log 写好之后会把 assert 断言换成日志？当然也不好说，毕竟断言可以直接编译失败，并且断言也可以打印错误 by Buns_ at 2026/10/17 16:52

  Ctrl_Base_Init(&controller_linear->controller_base, name, &controller_ops_linear);

  // 填写 controller_base
  controller_linear->b                     = b;
  controller_linear->k                     = k;
  controller_linear->is_defriction_enabled = is_defriction_enabled;
  controller_linear->max_out               = max_out;
  controller_linear->min_out               = min_out;
  controller_linear->max_target            = max_target;
  controller_linear->min_target            = min_target;
  controller_linear->max_step              = max_step;
  controller_linear->min_step              = min_step;
  controller_linear->deadzone              = deadzone;
  controller_linear->last_out              = 0;
}
