//
// 由 Buns_ 创建于 2026/10/17。
//

/**
 * @file controller_linear.c
 * @brief 线性控制律实现。
 * @ingroup controller
 */

#include "controller_linear.h"
#include "system_math.h"
#include <assert.h>
#include <stddef.h>

/**
 * @brief 按线性比例更新输出并处理饱和。
 * @param controller_h 已初始化的线性控制律基对象，不能为 NULL。
 * @param target 目标轮速，单位 m/s；必须有限，超出 [min_target, max_target] 时夹紧。
 * @param measure 未使用的实测量；仅用于满足 SISO 接口，本控制律忽略。
 * @param dt 未使用的控制周期，单位 s；接口要求有限且大于 0，本控制律忽略。
 * @param out 调用方拥有的输出存储，不能为 NULL；写入 duty 结果。
 * @retval CONTROLLER_STATUS_OK 未触发输入/输出限幅或最小步长调整；保持带可能已把输出保持在 last_out。
 * @retval CONTROLLER_STATUS_SATURATED target、绝对输出、相对步长或最小步长触发了调整。
 * @retval CONTROLLER_STATUS_ERROR target 或最终输出不为有限值；last_out 与 out 清零。
 * @pre 控制律已初始化；last_out 表示上一周期输出，out 可写。
 * @post last_out 与 out 同步为本次输出；输出保持带围绕旧 last_out 计算。
 * @note 开环映射，不使用 measure 和 dt；同一实例只允许单写者调用。
 */
static controller_status_t ctrl_linear_update(
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

/**
 * @brief 重置线性控制器的输出状态。
 * @param controller_h 已初始化的线性控制律基对象，不能为 NULL。
 * @pre 当前没有并发更新；配置参数无需重新初始化。
 * @post last_out 清零，下一周期从零输出开始计算步长和保持带。
 * @note 不修改 k、b、限幅和死区参数；不分配动态内存。
 */
static void ctrl_linear_reset(controller_base_t *controller_h) {
  assert(controller_h);
  controller_linear_t *controller_linear_h = (controller_linear_t *) controller_h;
  controller_linear_h->last_out = 0;
}

static const controller_ops_t controller_ops_linear = {
  .update = ctrl_linear_update,
  .reset  = ctrl_linear_reset,
};

/**
 * @brief 初始化线性控制律。
 * @param controller_h 待初始化的线性控制律存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；生命周期必须覆盖控制律对象。
 * @param k 输入到输出的线性斜率，单位 duty/(m/s)，必须有限。
 * @param b 截距或摩擦补偿幅值，单位 duty，必须有限。
 * @param is_defriction_enabled true 时按线性输出符号施加 b，false 时始终加 b。
 * @param max_target 输入上限，单位 m/s，必须有限且不小于 min_target。
 * @param min_target 输入下限，单位 m/s，必须有限且不大于 max_target。
 * @param max_out 输出上限，单位 duty，必须有限且非负。
 * @param min_out 输出下限，单位 duty，必须有限且非正。
 * @param max_step 单次更新最大变化量，单位 duty，必须有限且非负、不小于 min_step。
 * @param min_step 单次更新最小变化量，单位 duty，必须有限且非负、不大于 max_step。
 * @param deadzone 围绕 last_out 的输出保持带，单位 duty，必须有限、非负且小于 max_step。
 * @pre 所有浮点参数必须有限；范围不变量由 Debug 构建 assert 检查。
 * @post 控制律参数和 ops 已绑定，last_out 清零；不分配动态内存。
 * @note 仅允许初始化阶段单线程调用，不得与更新或复位并发。
 */
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
