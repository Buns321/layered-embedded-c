//
// Created by Buns_ on 2026/8/30.
//

#include "encoder_tim.h"
#include "platform_encoder.h"
#include "assert.h"

static int32_t dev_encoder_tim_readdelta(encoder_base_t *this) {
  assert(this);
  encoder_tim_t *self = (encoder_tim_t*)this;
  uint32_t now = Platform_Encoder_GetCounter(self->tim);
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

  self->last_count = now;
  self->position += delta;
  return delta;
}

static int32_t Dev_Encoder_TIM_GetPosition(encoder_base_t *this) {
  assert(this);
  encoder_tim_t *self = (encoder_tim_t*)this;
  return self->position;
}

static void Dev_Encoder_TIM_SetZero(encoder_base_t *this) {
  assert(this);
  encoder_tim_t *self = (encoder_tim_t*)this;
  Platform_Encoder_SetCounter(self->tim, 0);
  self->last_count = 0;
  self->has_base_count = true;
  self->position = 0;
}

static int32_t Dev_Encoder_TIM_GetDelta(encoder_base_t *this) {
  assert(this);
  return dev_encoder_tim_readdelta(this);
}

static void Dev_Encoder_TIM_Resync(encoder_base_t *this) {
  assert(this);
  dev_encoder_tim_readdelta(this);
}

static const encoder_ops_t encoder_ops_tim = {
  .get_position = Dev_Encoder_TIM_GetPosition,
  .set_zero = Dev_Encoder_TIM_SetZero,
  .get_delta = Dev_Encoder_TIM_GetDelta,
  .resync = Dev_Encoder_TIM_Resync,
};

void Dev_Encoder_TIM_Init(encoder_tim_t *this, const char *name, Platform_TIM_Instance tim, Platform_TIM_Channel channel) {
  assert(this && name);

  this->tim = tim;
  this->channel = channel;
  this->bit_width = Platform_Encoder_GetCounterBits(this->tim);

  assert(this->bit_width == 16 || this->bit_width == 32);

  Dev_Encoder_Base_Init(&this->encoder_base, name, &encoder_ops_tim);

  this->position = 0;
  this->last_count = 0;
  this->has_base_count = false;

  if (Platform_Encoder_Start(this->tim, this->channel) != Platform_OK) assert(0);
}
