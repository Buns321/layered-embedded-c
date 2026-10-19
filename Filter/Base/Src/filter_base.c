//
// Created by Buns_ on 2026/10/20.
//

#include "filter_base.h"
#include <assert.h>

void Filter_Base_Init(filter_base_t *this, const char *name, const filter_ops_t *ops) {
  assert(
    this        &&
    name        &&
    ops         &&
    ops->update &&
    ops->reset
  );
  this->name = name;
  this->ops  = ops;
}

void Filter_Base_Update(filter_base_t *this, const filter_sample_t *input, filter_sample_t *output) {
  assert(
    this              &&
    this->ops         &&
    this->ops->update &&
    input             &&
    output            &&
    input != output
  );
  assert((input->valid_mask & ~FILTER_CHANNEL_MASK) == 0u);

  *output = *input;
  this->ops->update(this, input, output);

  assert((output->valid_mask & ~FILTER_CHANNEL_MASK) == 0u);
  assert(output->time_ms == input->time_ms);
}

void Filter_Base_Reset(filter_base_t *this) {
  assert(this && this->ops && this->ops->reset);
  this->ops->reset(this);
}
