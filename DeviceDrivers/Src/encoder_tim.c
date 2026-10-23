//
// 由 Buns_ 创建于 2026/8/30。
//

#include "encoder_tim.h"
#include "platform_encoder.h"
#include "assert.h"

static int32_t dev_encoder_tim_readdelta(encoder_base_t *this) {
  assert(this);
  encoder_tim_t *self = (encoder_tim_t*)this;
  uint32_t now = 0u;
  if (Platform_Encoder_GetCounter(self->tim, &now) != Platform_OK) {
    self->overflow = true;
    return 0;
  }
  if (self->has_base_count == false) {
    self->last_count = now;
    self->has_base_count = true;
    return 0;
  }

  uint32_t diff = now - self->last_count;
  int32_t delta;

  if (self->bit_width == 16U) {
    diff &= 0xFFFFU;
    if (diff >= 0x8000U) {
      delta = (int32_t)((int64_t)diff - 0x10000LL);
    } else {
      delta = (int32_t)diff;
    }
  } else {
    if (diff >= 0x80000000U) {
      delta = (int32_t)((int64_t)diff - 0x100000000LL);
    } else {
      delta = (int32_t)diff;
    }
  }

  if (self->max_counts_per_period != 0u) {
    int64_t delta64 = (int64_t)delta;
    int64_t limit = (int64_t)self->max_counts_per_period;

    if (delta64 > limit) {
      delta64 = limit;
      self->overflow = true;
    } else if (delta64 < -limit) {
      delta64 = -limit;
      self->overflow = true;
    }

    delta = (int32_t)delta64;
  }

  self->last_count = now;
  self->position += delta;
  return delta;
}

static int64_t Dev_Encoder_TIM_GetPosition(encoder_base_t *this) {
  assert(this);
  encoder_tim_t *self = (encoder_tim_t*)this;
  return self->position;
}

static void Dev_Encoder_TIM_SetZero(encoder_base_t *this) {
  assert(this);
  encoder_tim_t *self = (encoder_tim_t*)this;
  if (Platform_Encoder_SetCounter(self->tim, 0u) != Platform_OK) {
    self->overflow = true;
    return;
  }
  self->last_count = 0;
  self->has_base_count = true;
  self->overflow = false;
  self->position = 0;
}

static int32_t Dev_Encoder_TIM_GetDelta(encoder_base_t *this) {
  assert(this);
  return dev_encoder_tim_readdelta(this);
}

static void Dev_Encoder_TIM_Resync(encoder_base_t *this) {
  assert(this);
  encoder_tim_t *self = (encoder_tim_t*)this;
  uint32_t counter = 0u;
  if (Platform_Encoder_GetCounter(self->tim, &counter) != Platform_OK) {
    self->overflow = true;
    return;
  }
  self->last_count = counter;
  self->has_base_count = true;
  self->overflow = false;
}

static const encoder_ops_t encoder_ops_tim = {
  .get_position = Dev_Encoder_TIM_GetPosition,
  .set_zero = Dev_Encoder_TIM_SetZero,
  .get_delta = Dev_Encoder_TIM_GetDelta,
  .resync = Dev_Encoder_TIM_Resync,
};

Platform_StatusTypeDef Dev_Encoder_TIM_Init(
  encoder_tim_t *this,
  const char *name,
  Platform_Encoder_Instance tim
) {
  assert(this && name);

  this->tim = tim;
  Platform_StatusTypeDef status = Platform_Encoder_GetCounterBits(
    this->tim,
    &this->bit_width
  );
  if (status != Platform_OK) {
    return status;
  }

  assert(this->bit_width == 16U || this->bit_width == 32U);

  Dev_Encoder_Base_Init(&this->encoder_base, name, &encoder_ops_tim);

  this->position = 0;
  this->last_count = 0;
  this->has_base_count = false;
  this->overflow = false;
  this->max_counts_per_period = 0u;

  return Platform_Encoder_Start(this->tim);
}

void Dev_Encoder_TIM_SetMaxCountsPerPeriod(encoder_tim_t *this, uint32_t max_counts) {
  assert(this);
  this->max_counts_per_period = max_counts;
}

bool Dev_Encoder_TIM_HasOverflow(const encoder_tim_t *this) {
  assert(this);
  return this->overflow;
}

void Dev_Encoder_TIM_ClearOverflow(encoder_tim_t *this) {
  assert(this);
  this->overflow = false;
}
