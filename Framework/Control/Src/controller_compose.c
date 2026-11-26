/**
 * @file controller_compose.c
 * @brief 并联求和控制律实现。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，完全静态分配。
 *          将同一输入（target、measure、dt_s）并发馈送至各子控制律并累加其输出，
 *          任一级报错即原子清零总输出并立即中止。
 * @ingroup Framework_Control
 */

#include "controller_compose.h"
#include "sys_math.h"
#include <stddef.h>

/**
 * @brief 组合控制律虚表 update 回调：依次计算各子控制律并累加输出。
 * @param[in,out] controller_h 组合控制律基类指针（实际类型为 controller_compose_t*）。
 * @param[in]     target       期望目标值（单位：m/s）。
 * @param[in]     measure      当前测量值（单位：m/s）。
 * @param[in]     dt_s         控制周期（单位：s，须 > 0）。
 * @param[out]    out          并联求和后的总输出指针（单位：duty）。
 * @return 执行状态：任一级 ERROR 则返回 ERROR；否则任一级 SATURATED 则返回 SATURATED；其余返回 OK。
 */
static controller_status_t ctrl_compose_update(controller_base_t *controller_h, float target,
                                               float measure, float dt_s, float *out) {
  controller_compose_t *compose = (controller_compose_t *)controller_h;
  // 边界保护：校验对象、级数上限及全部标量输入有限性，异常时安全清零输出
  if (compose == NULL || out == NULL || compose->stage_count == 0U ||
      compose->stage_count > CONTROLLER_COMPOSE_CAPACITY || !SYS_ISFINITE(target) ||
      !SYS_ISFINITE(measure) || !SYS_IS_POS_FINITE(dt_s)) {
    if (out != NULL) {
      *out = 0.0f;
    }
    return CONTROLLER_STATUS_ERROR;
  }

  float sum = 0.0f;
  controller_status_t status = CONTROLLER_STATUS_OK;

  for (uint8_t i = 0U; i < compose->stage_count; ++i) {
    float stage_out = 0.0f;
    controller_status_t stage_status =
        Ctrl_Base_Update(compose->stages[i], target, measure, dt_s, &stage_out);
    // 短路保护：只要任一子控制律失败，立即丢弃已累加部分并清零输出
    if (stage_status == CONTROLLER_STATUS_ERROR) {
      *out = 0.0f;
      return CONTROLLER_STATUS_ERROR;
    }
    // 饱和状态向上传播，便于上层监控前馈或反馈是否触顶
    if (stage_status == CONTROLLER_STATUS_SATURATED) {
      status = CONTROLLER_STATUS_SATURATED;
    }
    sum += stage_out;
  }

  // 防止多级输出相加后产生浮点溢出（Inf）
  if (!SYS_ISFINITE(sum)) {
    *out = 0.0f;
    return CONTROLLER_STATUS_ERROR;
  }

  *out = sum;
  return status;
}

/**
 * @brief 组合控制律虚表 reset 回调：按顺序复位全部子控制律。
 * @param[in,out] controller_h 组合控制律基类指针。
 */
static void ctrl_compose_reset(controller_base_t *controller_h) {
  controller_compose_t *compose = (controller_compose_t *)controller_h;
  if (compose == NULL || compose->stage_count > CONTROLLER_COMPOSE_CAPACITY) {
    return;
  }
  for (uint8_t i = 0U; i < compose->stage_count; ++i) {
    Ctrl_Base_Reset(compose->stages[i]);
  }
}

/** @brief 并联组合控制律静态只读虚表。 */
static const controller_ops_t s_ctrl_compose_ops = {
    .update = ctrl_compose_update,
    .reset = ctrl_compose_reset,
};

bool Ctrl_Compose_Init(controller_compose_t *compose, const char *name,
                       controller_base_t *const *stages, uint8_t stage_count) {
  if (compose == NULL || name == NULL || stages == NULL || stage_count == 0U ||
      stage_count > CONTROLLER_COMPOSE_CAPACITY) {
    return false;
  }

  // 全量预校验：拒绝 NULL、自引用（防止无限递归）、未初始化子项及重复指针
  for (uint8_t i = 0U; i < stage_count; ++i) {
    const controller_base_t *stage = stages[i];
    if (stage == NULL || stage == &compose->base || stage->ops == NULL ||
        stage->ops->update == NULL || stage->ops->reset == NULL) {
      return false;
    }
    for (uint8_t j = 0U; j < i; ++j) {
      if (stage == stages[j]) {
        return false;
      }
    }
  }

  // 全部校验通过后再写入成员并统一复位子级，确保失败时不污染 compose 原状态
  Ctrl_Base_Init(&compose->base, name, &s_ctrl_compose_ops);
  for (uint8_t i = 0U; i < CONTROLLER_COMPOSE_CAPACITY; ++i) {
    compose->stages[i] = (i < stage_count) ? stages[i] : NULL;
  }
  compose->stage_count = stage_count;
  Ctrl_Base_Reset(&compose->base);
  return true;
}
