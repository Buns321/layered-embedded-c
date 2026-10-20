//
// Created by Buns_ on 2026/10/20.
//

#include "filter_passthrough.h"
#include <assert.h>

static void Filter_Passthrough_Update(
  filter_base_t *this,
  const filter_sample_t *input,
  filter_sample_t *output
) {
  assert(this && input && output);

  (void)this;
  (void)input;
  (void)output;
}

static void Filter_Passthrough_Reset(filter_base_t *this) {
  assert(this);

  (void)this;
}

static const filter_ops_t filter_ops_passthrough = {
  .update = Filter_Passthrough_Update,
  .reset  = Filter_Passthrough_Reset,
};

void Filter_Passthrough_Init(
  filter_passthrough_t *this,
  const char *name
) {
  assert(this && name);

  Filter_Base_Init(&this->filter_base, name, &filter_ops_passthrough);
}
