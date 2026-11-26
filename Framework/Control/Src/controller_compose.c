/**
 * @file controller_compose.c
 * @brief 并联求和控制律实现。
 * @ingroup control
 */

#include "controller_compose.h"
#include "sys_math.h"
#include <stddef.h>

static controller_status_t ctrl_compose_update(controller_base_t *controller_h, float target,
                                               float measure, float dt_s, float *out) {
  controller_compose_t *compose = (controller_compose_t *)controller_h;
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
    if (stage_status == CONTROLLER_STATUS_ERROR) {
      *out = 0.0f;
      return CONTROLLER_STATUS_ERROR;
    }
    if (stage_status == CONTROLLER_STATUS_SATURATED) {
      status = CONTROLLER_STATUS_SATURATED;
    }
    sum += stage_out;
  }

  if (!SYS_ISFINITE(sum)) {
    *out = 0.0f;
    return CONTROLLER_STATUS_ERROR;
  }

  *out = sum;
  return status;
}

static void ctrl_compose_reset(controller_base_t *controller_h) {
  controller_compose_t *compose = (controller_compose_t *)controller_h;
  if (compose == NULL || compose->stage_count > CONTROLLER_COMPOSE_CAPACITY) {
    return;
  }
  for (uint8_t i = 0U; i < compose->stage_count; ++i) {
    Ctrl_Base_Reset(compose->stages[i]);
  }
}

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

  Ctrl_Base_Init(&compose->base, name, &s_ctrl_compose_ops);
  for (uint8_t i = 0U; i < CONTROLLER_COMPOSE_CAPACITY; ++i) {
    compose->stages[i] = (i < stage_count) ? stages[i] : NULL;
  }
  compose->stage_count = stage_count;
  Ctrl_Base_Reset(&compose->base);
  return true;
}
