//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file filter_chain.c
 * @brief 固定容量滤波器链实现。
 * @ingroup filter
 */

#include "filter_chain.h"
#include <stddef.h>
#include <assert.h>

static void Filter_Chain_Reset(filter_base_t *filter_h) {
  assert(filter_h);
  filter_chain_t *filter_chain_h = (filter_chain_t *)filter_h;

  assert(filter_chain_h->stage_count > 0u);
  assert(filter_chain_h->stage_count <= FILTER_CHAIN_CAPACITY);

  for (uint8_t i = 0; i < filter_chain_h->stage_count; i++) {
    assert(filter_chain_h->filter_stage_h[i]);
    Filter_Base_Reset(filter_chain_h->filter_stage_h[i]);
  }
}

static void Filter_Chain_Update(filter_base_t *filter_h, const filter_sample_t *input, filter_sample_t *output) {
  assert(filter_h && input && output);
  filter_chain_t *filter_chain_h = (filter_chain_t *)filter_h;

  assert(filter_chain_h->stage_count > 0u);
  assert(filter_chain_h->stage_count <= FILTER_CHAIN_CAPACITY);

  filter_sample_t scratch_a;
  filter_sample_t scratch_b;
  const filter_sample_t *stage_input = input;

  for (uint8_t i = 0; i < filter_chain_h->stage_count; i++) {
    filter_sample_t *stage_output =
      ((i & 1u) == 0u) ? &scratch_a : &scratch_b;

    assert(filter_chain_h->filter_stage_h[i]);
    Filter_Base_Update(filter_chain_h->filter_stage_h[i], stage_input, stage_output);
    stage_input = stage_output;
  }

  *output = *stage_input;
}

static const filter_ops_t filter_ops_chain = {
  .update = Filter_Chain_Update,
  .reset  = Filter_Chain_Reset,
};

void Filter_Chain_Init(filter_chain_t *filter_h, const char *name, filter_base_t *const *filter_stage_h, uint8_t stage_count) {
  assert(
    filter_h                                 &&
    name                                 &&
    filter_stage_h                               &&
    stage_count > 0u                     &&
    stage_count <= FILTER_CHAIN_CAPACITY
  );

  for (uint8_t i = 0; i < stage_count; i++) {
    assert(
      filter_stage_h[i]                       &&
      filter_stage_h[i] != &filter_h->filter_base &&
      filter_stage_h[i]->ops                  &&
      filter_stage_h[i]->ops->update          &&
      filter_stage_h[i]->ops->reset
    );

    for (uint8_t j = 0; j < i; j++) {
      assert(filter_stage_h[i] != filter_stage_h[j]);
    }
  }

  Filter_Base_Init(&filter_h->filter_base, name, &filter_ops_chain);

  for (uint8_t i = 0; i < FILTER_CHAIN_CAPACITY; i++) {
    filter_h->filter_stage_h[i] = NULL;
  }

  for (uint8_t i = 0; i < stage_count; i++) {
    filter_h->filter_stage_h[i] = filter_stage_h[i];
  }

  filter_h->stage_count = stage_count;

  Filter_Base_Reset(&filter_h->filter_base);
}
