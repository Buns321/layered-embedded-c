/**
 * @file filter_chain.c
 * @brief 固定容量串联滤波器链实现。
 * @ingroup control
 */

#include "filter_chain.h"
#include <stddef.h>

typedef struct filter_chain_init_data_s {
  filter_base_t *const *stages;
  uint8_t stage_count;
} filter_chain_init_data_t;

static filter_status_t filter_chain_init(filter_base_t *filter_h,
                                         const filter_config_t *filter_config) {
  filter_chain_t *chain = (filter_chain_t *)filter_h;
  const filter_chain_init_data_t *init_data = (const filter_chain_init_data_t *)filter_config->data;

  if (init_data == NULL || init_data->stages == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }
  if (init_data->stage_count == 0U || init_data->stage_count > FILTER_CHAIN_CAPACITY) {
    return FILTER_STATUS_INVALID_CONFIG;
  }

  for (uint8_t i = 0U; i < init_data->stage_count; ++i) {
    const filter_base_t *stage_h = init_data->stages[i];
    if (stage_h == NULL || stage_h == filter_h || stage_h->ops == NULL) {
      return FILTER_STATUS_INVALID_CONFIG;
    }
    for (uint8_t j = 0U; j < i; ++j) {
      if (stage_h == init_data->stages[j]) {
        return FILTER_STATUS_INVALID_CONFIG;
      }
    }
  }

  for (uint8_t i = 0U; i < FILTER_CHAIN_CAPACITY; ++i) {
    chain->stages[i] = (i < init_data->stage_count) ? init_data->stages[i] : NULL;
  }
  chain->stage_count = init_data->stage_count;
  return FILTER_STATUS_OK;
}

static void filter_chain_reset(filter_base_t *filter_h) {
  filter_chain_t *chain = (filter_chain_t *)filter_h;
  for (uint8_t i = 0U; i < chain->stage_count; ++i) {
    Filter_Base_Reset(chain->stages[i]);
  }
}

static void filter_chain_update(filter_base_t *filter_h, filter_sample_t *sample) {
  filter_chain_t *chain = (filter_chain_t *)filter_h;
  filter_sample_t scratch_a;
  filter_sample_t scratch_b;
  const filter_sample_t *stage_input = sample;

  for (uint8_t i = 0U; i < chain->stage_count; ++i) {
    filter_sample_t *stage_output = ((i & 1U) == 0U) ? &scratch_a : &scratch_b;
    Filter_Base_Update(chain->stages[i], stage_input, stage_output);
    stage_input = stage_output;
  }

  *sample = *stage_input;
}

static const filter_ops_t s_filter_ops_chain = {
    .init = filter_chain_init,
    .update = filter_chain_update,
    .reset = filter_chain_reset,
};

filter_status_t Filter_Chain_Init(filter_chain_t *filter_h, const char *name,
                                  filter_base_t *const *stages, uint8_t stage_count) {
  if (filter_h == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }
  const filter_chain_init_data_t init_data = {
      .stages = stages,
      .stage_count = stage_count,
  };
  const filter_config_t config = {
      .name = name,
      .data = &init_data,
  };
  return Filter_Base_Construct(&filter_h->base, &config, &s_filter_ops_chain);
}
