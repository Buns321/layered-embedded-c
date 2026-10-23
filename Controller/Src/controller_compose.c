//
// 由 Buns_ 创建于 2026/10/20。
//

#include "controller_compose.h"
#include "system_math.h"
#include <assert.h>

static controller_status_t Ctrl_Compose_Update(
  controller_base_t *this,
  float target,
  float measure,
  float dt,
  float *out
) {
  assert(this && out);

  controller_compose_t *self = (controller_compose_t *)this;
  assert(self->stage_count > 0u);
  assert(self->stage_count <= CONTROLLER_COMPOSE_CAPACITY);

  float sum = 0.0f;
  controller_status_t status = CONTROLLER_STATUS_OK;

  for (uint8_t i = 0; i < self->stage_count; i++) {
    assert(self->stages[i]);

    float stage_out = 0.0f;
    controller_status_t stage_status = Ctrl_Base_Update(
      self->stages[i],
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

static void Ctrl_Compose_Reset(controller_base_t *this) {
  assert(this);

  controller_compose_t *self = (controller_compose_t *)this;
  assert(self->stage_count > 0u);
  assert(self->stage_count <= CONTROLLER_COMPOSE_CAPACITY);

  for (uint8_t i = 0; i < self->stage_count; i++) {
    assert(self->stages[i]);
    Ctrl_Base_Reset(self->stages[i]);
  }
}

static const controller_ops_t controller_ops_compose = {
  .update = Ctrl_Compose_Update,
  .reset  = Ctrl_Compose_Reset,
};

void Ctrl_Compose_Init(
  controller_compose_t      *this,
  const char                *name,
  controller_base_t *const  *stages,
  uint8_t                    stage_count
) {
  assert(
    this                                 &&
    name                                 &&
    stages                               &&
    stage_count > 0u                     &&
    stage_count <= CONTROLLER_COMPOSE_CAPACITY
  );

  for (uint8_t i = 0; i < stage_count; i++) {
    assert(
      stages[i]                       &&
      stages[i] != &this->base        &&
      stages[i]->ops                  &&
      stages[i]->ops->update          &&
      stages[i]->ops->reset
    );

    for (uint8_t j = 0; j < i; j++) {
      assert(stages[i] != stages[j]);
    }
  }

  Ctrl_Base_Init(&this->base, name, &controller_ops_compose);

  for (uint8_t i = 0; i < CONTROLLER_COMPOSE_CAPACITY; i++) {
    this->stages[i] = NULL;
  }

  for (uint8_t i = 0; i < stage_count; i++) {
    this->stages[i] = stages[i];
  }

  this->stage_count = stage_count;

  Ctrl_Base_Reset(&this->base);
}
