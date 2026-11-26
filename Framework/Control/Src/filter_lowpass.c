/**
 * @file filter_lowpass.c
 * @brief 多通道一阶低通滤波器实现。
 * @ingroup control
 */

#include "filter_lowpass.h"
#include "sys_math.h"

static filter_status_t filter_lowpass_init(filter_base_t *filter_h,
                                           const filter_config_t *filter_config) {
  filter_lowpass_t *lowpass = (filter_lowpass_t *)filter_h;
  const float *default_tau_s = (const float *)filter_config->data;

  if (default_tau_s == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }
  if (!SYS_IS_POS_FINITE(*default_tau_s)) {
    return FILTER_STATUS_INVALID_CONFIG;
  }

  for (uint8_t i = 0U; i < FILTER_CHANNEL_CAPACITY; ++i) {
    lowpass->time_constant_s[i] = *default_tau_s;
  }
  return FILTER_STATUS_OK;
}

static void filter_lowpass_reset(filter_base_t *filter_h) {
  filter_lowpass_t *lowpass = (filter_lowpass_t *)filter_h;
  lowpass->initialized_mask = 0U;
  for (uint8_t i = 0U; i < FILTER_CHANNEL_CAPACITY; ++i) {
    lowpass->last_output[i] = 0.0f;
    lowpass->last_time_ms[i] = 0U;
  }
}

static void filter_lowpass_update(filter_base_t *filter_h, filter_sample_t *sample) {
  filter_lowpass_t *lowpass = (filter_lowpass_t *)filter_h;

  for (uint8_t channel = 0U; channel < FILTER_CHANNEL_CAPACITY; ++channel) {
    uint32_t channel_mask = Filter_Sample_GetChannelMask(channel);
    if ((sample->valid_mask & channel_mask) == 0U) {
      continue;
    }

    float input_value = sample->value[channel];
    float tau_s = lowpass->time_constant_s[channel];

    if ((lowpass->initialized_mask & channel_mask) == 0U) {
      lowpass->last_output[channel] = input_value;
      lowpass->last_time_ms[channel] = sample->time_ms;
      lowpass->initialized_mask |= channel_mask;
      continue;
    }

    uint32_t elapsed_ms = sample->time_ms - lowpass->last_time_ms[channel];
    if (elapsed_ms == 0U) {
      sample->value[channel] = lowpass->last_output[channel];
      continue;
    }

    float dt_s = (float)elapsed_ms * 0.001f;
    float alpha = dt_s / (tau_s + dt_s);
    alpha = SYS_CLAMP(alpha, 0.0f, 1.0f);

    float output_value =
        lowpass->last_output[channel] + (alpha * (input_value - lowpass->last_output[channel]));

    if (!Filter_Sample_WriteFiniteChannel(sample, channel, output_value)) {
      continue;
    }

    lowpass->last_output[channel] = output_value;
    lowpass->last_time_ms[channel] = sample->time_ms;
  }
}

static const filter_ops_t s_filter_ops_lowpass = {
    .init = filter_lowpass_init,
    .update = filter_lowpass_update,
    .reset = filter_lowpass_reset,
};

filter_status_t Filter_Lowpass_Init(filter_lowpass_t *filter_h, const char *name,
                                    float default_time_constant_s) {
  if (filter_h == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }
  const filter_config_t config = {
      .name = name,
      .data = &default_time_constant_s,
  };
  return Filter_Base_Construct(&filter_h->base, &config, &s_filter_ops_lowpass);
}

filter_status_t Filter_Lowpass_SetChannelTimeConstant(filter_lowpass_t *filter_h, uint8_t channel,
                                                      float time_constant_s) {
  if (filter_h == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }
  if (channel >= FILTER_CHANNEL_CAPACITY || !SYS_IS_POS_FINITE(time_constant_s)) {
    return FILTER_STATUS_INVALID_CONFIG;
  }
  filter_h->time_constant_s[channel] = time_constant_s;
  return FILTER_STATUS_OK;
}
