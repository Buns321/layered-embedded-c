//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file filter_base.c
 * @brief 滤波器抽象接口实现。
 * @ingroup filter
 */

#include "filter_base.h"
#include <assert.h>

void Filter_Base_Init(filter_base_t *filter_h, const char *name, const filter_ops_t *ops) {
  assert(
    filter_h        &&
    name        &&
    ops         &&
    ops->update &&
    ops->reset
  );
  filter_h->name = name;
  filter_h->ops  = ops;
}

void Filter_Base_Update(filter_base_t *filter_h, const filter_sample_t *input, filter_sample_t *output) {
  assert(
    filter_h              &&
    filter_h->ops         &&
    filter_h->ops->update &&
    input             &&
    output            &&
    input != output
  );
  assert((input->valid_mask & ~FILTER_CHANNEL_MASK) == 0u);

  *output = *input;
  filter_h->ops->update(filter_h, input, output);

  assert((output->valid_mask & ~FILTER_CHANNEL_MASK) == 0u);
  assert(output->time_ms == input->time_ms);
}

void Filter_Base_Reset(filter_base_t *filter_h) {
  assert(filter_h && filter_h->ops && filter_h->ops->reset);
  filter_h->ops->reset(filter_h);
}
