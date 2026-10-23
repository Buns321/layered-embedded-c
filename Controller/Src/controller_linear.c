//
// 由 Buns_ 创建于 2026/10/17。
//

#include "controller_linear.h"
#include "system_math.h"
#include <assert.h>

static controller_status_t Ctrl_Linear_Update(
  controller_base_t *controller_h,
  float target,
  float measure,
  float dt,
  float *out
) {
  assert(controller_h && out);
  (void) measure;
  (void) dt;
  controller_linear_t *controller_linear_h = (controller_linear_t *) controller_h;

  if (!SYS_ISFINITE(target)) {
    controller_linear_h->last_out = 0.0f;
    *out = 0.0f;
    return CONTROLLER_STATUS_ERROR;
  }

  bool saturated = false;

  // 输入限幅
  float clamped_target = SYS_CLAMP(target, controller_linear_h->min_target, controller_linear_h->max_target);
  if (clamped_target != target) saturated = true;
  target = clamped_target;

  float result = target * controller_linear_h->k + controller_linear_h->b * (controller_linear_h->is_defriction_enabled ? SYS_SIGN(target * controller_linear_h->k) : 1.0f);

  // 绝对输出最大&最小限幅
  float clamped_out = SYS_CLAMP(result, controller_linear_h->min_out, controller_linear_h->max_out);
  if (clamped_out != result) saturated = true;
  result = clamped_out;

  // 相对输出最大限幅
  clamped_out = SYS_CLAMP(
    result,
    controller_linear_h->last_out - controller_linear_h->max_step,
    controller_linear_h->last_out + controller_linear_h->max_step
  );
  if (clamped_out != result) saturated = true;
  result = clamped_out;

  /* |-----------------|----------|----------|
   * | 死区 \ 最小步长 | FALSE    | TRUE     |
   * |-----------------|----------|----------|
   * | FALSE           | out      | min_step |
   * | TRUE            | last_out | last_out |
   * |-----------------|----------|----------|
   */

  // 输出死区
  if (
    result < controller_linear_h->last_out + controller_linear_h->deadzone &&
    result > controller_linear_h->last_out - controller_linear_h->deadzone
  ) result = controller_linear_h->last_out;

  // 相对输出最小限幅
  else if (
    result != controller_linear_h->last_out &&
    result < controller_linear_h->last_out + controller_linear_h->min_step &&
    result > controller_linear_h->last_out - controller_linear_h->min_step
  ) {
    result = controller_linear_h->last_out + (result > controller_linear_h->last_out ? +controller_linear_h->min_step : -controller_linear_h->min_step);
    saturated = true;
  }

  // 非有限输出保护
  if (!SYS_ISFINITE(result)) {
    controller_linear_h->last_out = 0.0f;
    *out = 0.0f;
    return CONTROLLER_STATUS_ERROR;
  }

  // 更新 last_out
  controller_linear_h->last_out = result;

  *out = result;
  return saturated ? CONTROLLER_STATUS_SATURATED : CONTROLLER_STATUS_OK;
}

static void Ctrl_Linear_Reset(controller_base_t *controller_h) {
  assert(controller_h);
  controller_linear_t *controller_linear_h = (controller_linear_t *) controller_h;
  controller_linear_h->last_out = 0;
}

static const controller_ops_t controller_ops_linear = {
  .update = Ctrl_Linear_Update,
  .reset  = Ctrl_Linear_Reset,
};

void Ctrl_Linear_Init(
  controller_linear_t *controller_h,
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
    controller_h != NULL            &&
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

  // TODO 由 Buns_ 于 2026/10/17 16:52 记录：待日志系统完善后，再评估是否把
  // assert 断言替换为日志；断言可以直接阻止错误代码继续运行，也可能输出错误信息。

  Ctrl_Base_Init(&controller_h->controller_base, name, &controller_ops_linear);

  // 填写 controller_base
  controller_h->b                     = b;
  controller_h->k                     = k;
  controller_h->is_defriction_enabled = is_defriction_enabled;
  controller_h->max_out               = max_out;
  controller_h->min_out               = min_out;
  controller_h->max_target            = max_target;
  controller_h->min_target            = min_target;
  controller_h->max_step              = max_step;
  controller_h->min_step              = min_step;
  controller_h->deadzone              = deadzone;
  controller_h->last_out              = 0;
}
