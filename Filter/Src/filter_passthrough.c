//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file filter_passthrough.c
 * @brief 多通道透传滤波器实现。
 * @ingroup filter
 */

#include "filter_passthrough.h"
#include <assert.h>

static void filter_passthrough_update(
  filter_base_t *filter_h,
  const filter_sample_t *input,
  filter_sample_t *output
) {
  assert(filter_h && input && output);

  (void)filter_h;
  (void)input;
  (void)output;
}

static void filter_passthrough_reset(filter_base_t *filter_h) {
  assert(filter_h);

  (void)filter_h;
}

static const filter_ops_t filter_ops_passthrough = {
  .update = filter_passthrough_update,
  .reset  = filter_passthrough_reset,
};

void Filter_Passthrough_Init(
  filter_passthrough_t *filter_h,
  const char *name
) {
  assert(filter_h && name);

  Filter_Base_Init(&filter_h->filter_base, name, &filter_ops_passthrough);
}
