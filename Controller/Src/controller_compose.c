//
// 由 Buns_ 创建于 2026/10/20。
//

#include "controller_compose.h"
#include "system_math.h"
#include <assert.h>

static controller_status_t Ctrl_Compose_Update(
  controller_base_t *controller_h,
  float target,
  float measure,
  float dt,
  float *out
) {
  assert(controller_h && out);

  controller_compose_t *controller_compose_h = (controller_compose_t *)controller_h;
  assert(controller_compose_h->stage_count > 0u);
  assert(controller_compose_h->stage_count <= CONTROLLER_COMPOSE_CAPACITY);

  float sum = 0.0f;
  controller_status_t status = CONTROLLER_STATUS_OK;

  for (uint8_t i = 0; i < controller_compose_h->stage_count; i++) {
    assert(controller_compose_h->controller_stage_h[i]);

    float stage_out = 0.0f;
    controller_status_t stage_status = Ctrl_Base_Update(
      controller_compose_h->controller_stage_h[i],
      target,
      measure,
      dt,
      &stage_out
    );

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

static void Ctrl_Compose_Reset(controller_base_t *controller_h) {
  assert(controller_h);

  controller_compose_t *controller_compose_h = (controller_compose_t *)controller_h;
  assert(controller_compose_h->stage_count > 0u);
  assert(controller_compose_h->stage_count <= CONTROLLER_COMPOSE_CAPACITY);

  for (uint8_t i = 0; i < controller_compose_h->stage_count; i++) {
    assert(controller_compose_h->controller_stage_h[i]);
    Ctrl_Base_Reset(controller_compose_h->controller_stage_h[i]);
  }
}

static const controller_ops_t controller_ops_compose = {
  .update = Ctrl_Compose_Update,
  .reset  = Ctrl_Compose_Reset,
};

void Ctrl_Compose_Init(
  controller_compose_t      *controller_h,
  const char                *name,
  controller_base_t *const  *controller_stage_h,
  uint8_t                    stage_count
) {
  assert(
    controller_h                                 &&
    name                                 &&
    controller_stage_h                               &&
    stage_count > 0u                     &&
    stage_count <= CONTROLLER_COMPOSE_CAPACITY
  );

  for (uint8_t i = 0; i < stage_count; i++) {
    assert(
      controller_stage_h[i]                       &&
      controller_stage_h[i] != &controller_h->base        &&
      controller_stage_h[i]->ops                  &&
      controller_stage_h[i]->ops->update          &&
      controller_stage_h[i]->ops->reset
    );

    for (uint8_t j = 0; j < i; j++) {
      assert(controller_stage_h[i] != controller_stage_h[j]);
    }
  }

  Ctrl_Base_Init(&controller_h->base, name, &controller_ops_compose);

  for (uint8_t i = 0; i < CONTROLLER_COMPOSE_CAPACITY; i++) {
    controller_h->controller_stage_h[i] = NULL;
  }

  for (uint8_t i = 0; i < stage_count; i++) {
    controller_h->controller_stage_h[i] = controller_stage_h[i];
  }

  controller_h->stage_count = stage_count;

  Ctrl_Base_Reset(&controller_h->base);
}
