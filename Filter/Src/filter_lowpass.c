//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file filter_lowpass.c
 * @brief 多通道一阶低通滤波器实现。
 * @ingroup filter
 */

#include "filter_lowpass.h"
#include "system_math.h"
#include <assert.h>

static void filter_lowpass_reset(filter_base_t *filter_h) {
  assert(filter_h);
  filter_lowpass_t *filter_lowpass_h = (filter_lowpass_t *)filter_h;

  filter_lowpass_h->initialized_mask = 0u;

  for (uint8_t i = 0; i < FILTER_CHANNEL_CAPACITY; i++) {
    filter_lowpass_h->last_output[i]  = 0.0f;
    filter_lowpass_h->last_time_ms[i] = 0u;
  }
}

static void filter_lowpass_update(filter_base_t *filter_h, const filter_sample_t *input, filter_sample_t *output) {
  assert(filter_h && input && output);
  filter_lowpass_t *filter_lowpass_h = (filter_lowpass_t *)filter_h;

  for (uint8_t channel = 0; channel < FILTER_CHANNEL_CAPACITY; channel++) {
    uint32_t channel_mask = (uint32_t)1u << channel;
    if ((input->valid_mask & channel_mask) == 0u) {
      continue;
    }

    float input_value     = input->value[channel];
    float time_constant_s = filter_lowpass_h->time_constant_s[channel];

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

    if ((filter_lowpass_h->initialized_mask & channel_mask) == 0u) {
      filter_lowpass_h->last_output[channel]  = input_value;
      filter_lowpass_h->last_time_ms[channel] = input->time_ms;
      filter_lowpass_h->initialized_mask     |= channel_mask;
      output->value[channel]      = input_value;
      continue;
    }

    uint32_t elapsed_ms = input->time_ms - filter_lowpass_h->last_time_ms[channel];
    if (elapsed_ms == 0u) {
      output->value[channel] = filter_lowpass_h->last_output[channel];
      continue;
    }

    float dt_s = (float)elapsed_ms * 0.001f;
    float alpha = dt_s / (time_constant_s + dt_s);
    alpha = SYS_CLAMP(alpha, 0.0f, 1.0f);

    float output_value = filter_lowpass_h->last_output[channel] +
                         alpha * (input_value - filter_lowpass_h->last_output[channel]);

    if (!SYS_ISFINITE(output_value)) {
      output->valid_mask &= ~channel_mask;
      continue;
    }

    filter_lowpass_h->last_output[channel]  = output_value;
    filter_lowpass_h->last_time_ms[channel] = input->time_ms;
    output->value[channel]      = output_value;
  }
}

static const filter_ops_t filter_ops_lowpass = {
  .update = filter_lowpass_update,
  .reset  = filter_lowpass_reset,
};

void Filter_Lowpass_Init(filter_lowpass_t *filter_h, const char *name, float default_time_constant_s) {
  assert(
    filter_h                                  &&
    name                                  &&
    SYS_ISFINITE(default_time_constant_s) &&
    default_time_constant_s > 0.0f
  );

  Filter_Base_Init(&filter_h->filter_base, name, &filter_ops_lowpass);

  for (uint8_t i = 0; i < FILTER_CHANNEL_CAPACITY; i++) {
    filter_h->time_constant_s[i] = default_time_constant_s;
  }

  Filter_Base_Reset(&filter_h->filter_base);
}

void Filter_Lowpass_SetChannelTimeConstant(filter_lowpass_t *filter_h, uint8_t channel, float time_constant_s) {
  assert(
    filter_h                              &&
    channel < FILTER_CHANNEL_CAPACITY &&
    SYS_ISFINITE(time_constant_s)     &&
    time_constant_s > 0.0f
  );
  filter_h->time_constant_s[channel] = time_constant_s;
}
