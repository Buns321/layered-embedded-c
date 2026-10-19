//
// Created by Buns_ on 2026/10/20.
//

#include "filter_lowpass.h"
#include "system_math.h"
#include <assert.h>

static void Filter_Lowpass_Reset(filter_base_t *this) {
  assert(this);
  filter_lowpass_t *self = (filter_lowpass_t *)this;

  self->initialized_mask = 0u;

  for (uint8_t i = 0; i < FILTER_CHANNEL_CAPACITY; i++) {
    self->last_output[i]  = 0.0f;
    self->last_time_ms[i] = 0u;
  }
}

static void Filter_Lowpass_Update(filter_base_t *this, const filter_sample_t *input, filter_sample_t *output) {
  assert(this && input && output);
  filter_lowpass_t *self = (filter_lowpass_t *)this;

  for (uint8_t channel = 0; channel < FILTER_CHANNEL_CAPACITY; channel++) {
    uint32_t channel_mask = (uint32_t)1u << channel;
    if ((input->valid_mask & channel_mask) == 0u) {
      continue;
    }

    float input_value     = input->value[channel];
    float time_constant_s = self->time_constant_s[channel];

    assert(
      SYS_ISFINITE(input_value)     &&
      SYS_ISFINITE(time_constant_s) &&
      time_constant_s > 0.0f
    );

    if (
      !SYS_ISFINITE(input_value)     ||
      !SYS_ISFINITE(time_constant_s) ||
      time_constant_s <= 0.0f
    ) {
      output->valid_mask &= ~channel_mask;
      continue;
    }

    if ((self->initialized_mask & channel_mask) == 0u) {
      self->last_output[channel]  = input_value;
      self->last_time_ms[channel] = input->time_ms;
      self->initialized_mask     |= channel_mask;
      output->value[channel]      = input_value;
      continue;
    }

    uint32_t elapsed_ms = input->time_ms - self->last_time_ms[channel];
    if (elapsed_ms == 0u) {
      output->value[channel] = self->last_output[channel];
      continue;
    }

    float dt_s = (float)elapsed_ms * 0.001f;
    float alpha = dt_s / (time_constant_s + dt_s);
    alpha = SYS_CLAMP(alpha, 0.0f, 1.0f);

    float output_value = self->last_output[channel] +
                         alpha * (input_value - self->last_output[channel]);

    if (!SYS_ISFINITE(output_value)) {
      output->valid_mask &= ~channel_mask;
      continue;
    }

    self->last_output[channel]  = output_value;
    self->last_time_ms[channel] = input->time_ms;
    output->value[channel]      = output_value;
  }
}

static const filter_ops_t filter_ops_lowpass = {
  .update = Filter_Lowpass_Update,
  .reset  = Filter_Lowpass_Reset,
};

void Filter_Lowpass_Init(filter_lowpass_t *this, const char *name, float default_time_constant_s) {
  assert(
    this                                  &&
    name                                  &&
    SYS_ISFINITE(default_time_constant_s) &&
    default_time_constant_s > 0.0f
  );

  Filter_Base_Init(&this->filter_base, name, &filter_ops_lowpass);

  for (uint8_t i = 0; i < FILTER_CHANNEL_CAPACITY; i++) {
    this->time_constant_s[i] = default_time_constant_s;
  }

  Filter_Base_Reset(&this->filter_base);
}

void Filter_Lowpass_SetChannelTimeConstant(filter_lowpass_t *this, uint8_t channel, float time_constant_s) {
  assert(
    this                              &&
    channel < FILTER_CHANNEL_CAPACITY &&
    SYS_ISFINITE(time_constant_s)     &&
    time_constant_s > 0.0f
  );
  this->time_constant_s[channel] = time_constant_s;
}
