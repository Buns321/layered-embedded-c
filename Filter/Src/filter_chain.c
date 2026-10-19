//
// Created by Buns_ on 2026/10/20.
//

#include "filter_chain.h"
#include <stddef.h>
#include <assert.h>

static void Filter_Chain_Reset(filter_base_t *this) {
  assert(this);
  filter_chain_t *self = (filter_chain_t *)this;

  assert(self->stage_count > 0u);
  assert(self->stage_count <= FILTER_CHAIN_CAPACITY);

  for (uint8_t i = 0; i < self->stage_count; i++) {
    assert(self->stages[i]);
    Filter_Base_Reset(self->stages[i]);
  }
}

static void Filter_Chain_Update(filter_base_t *this, const filter_sample_t *input, filter_sample_t *output) {
  assert(this && input && output);
  filter_chain_t *self = (filter_chain_t *)this;

  assert(self->stage_count > 0u);
  assert(self->stage_count <= FILTER_CHAIN_CAPACITY);

  filter_sample_t scratch_a;
  filter_sample_t scratch_b;
  const filter_sample_t *stage_input = input;

  for (uint8_t i = 0; i < self->stage_count; i++) {
    filter_sample_t *stage_output =
      ((i & 1u) == 0u) ? &scratch_a : &scratch_b;

    assert(self->stages[i]);
    Filter_Base_Update(self->stages[i], stage_input, stage_output);
    stage_input = stage_output;
  }

  *output = *stage_input;
}

static const filter_ops_t filter_ops_chain = {
  .update = Filter_Chain_Update,
  .reset  = Filter_Chain_Reset,
};

void Filter_Chain_Init(filter_chain_t *this, const char *name, filter_base_t *const *stages, uint8_t stage_count) {
  assert(
    this                                 &&
    name                                 &&
    stages                               &&
    stage_count > 0u                     &&
    stage_count <= FILTER_CHAIN_CAPACITY
  );

  for (uint8_t i = 0; i < stage_count; i++) {
    assert(
      stages[i]                       &&
      stages[i] != &this->filter_base &&
      stages[i]->ops                  &&
      stages[i]->ops->update          &&
      stages[i]->ops->reset
    );

    for (uint8_t j = 0; j < i; j++) {
      assert(stages[i] != stages[j]);
    }
  }

  Filter_Base_Init(&this->filter_base, name, &filter_ops_chain);

  for (uint8_t i = 0; i < FILTER_CHAIN_CAPACITY; i++) {
    this->stages[i] = NULL;
  }

  for (uint8_t i = 0; i < stage_count; i++) {
    this->stages[i] = stages[i];
  }

  this->stage_count = stage_count;

  Filter_Base_Reset(&this->filter_base);
}
