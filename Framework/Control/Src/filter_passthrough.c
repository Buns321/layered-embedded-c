/**
 * @file filter_passthrough.c
 * @brief 多通道透传滤波器实现。
 * @ingroup control
 */

#include "filter_passthrough.h"

static const filter_ops_t s_filter_ops_passthrough = {0};

filter_status_t Filter_Passthrough_Init(filter_passthrough_t *filter_h, const char *name) {
  if (filter_h == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }
  const filter_config_t config = {
      .name = name,
      .data = NULL,
  };
  return Filter_Base_Construct(&filter_h->base, &config, &s_filter_ops_passthrough);
}
