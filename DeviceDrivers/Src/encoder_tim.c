//
// Created by Buns_ on 2026/8/30.
//

#include "../Inc/encoder_tim.h"
#include "../../Platform/Inc/platform_encoder.h"
#include "assert.h"

static int32_t Dev_Encoder_TIM_GetPosition(encoder_base_t *this) {
  assert(this);
  encoder_tim_t *self = (encoder_tim_t*)this;
  uint32_t now = Platform_Encoder_GetCounter(self->tim);
  if (self->is_first) {self->last_count = now; self->is_first = false;}
  int32_t delta = now - self->last_count;
  self->last_count = now;
  self->position += delta;
  return self->position;
}

static void Dev_Encoder_TIM_SetZero(encoder_base_t *this) {
  assert(this);
  encoder_tim_t *self = (encoder_tim_t*)this;
  Platform_Encoder_SetCounter(self->tim, 0);
  self->last_count = 0;
  self->position = 0;
}

static const encoder_ops_t encoder_ops_tim = {
  .get_position = Dev_Encoder_TIM_GetPosition,
  .set_zero = Dev_Encoder_TIM_SetZero,
};

void Dev_Encoder_TIM_Init(encoder_tim_t *this, const char *name, Platform_TIM_Instance tim, Platform_TIM_Channel channel) {
  assert(this && name);
  Dev_Encoder_Base_Init(&this->encoder_base, name, &encoder_ops_tim);
  this->tim = tim;
  this->channel = channel;
  this->last_count = 0;
}
