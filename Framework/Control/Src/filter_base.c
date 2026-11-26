/**
 * @file filter_base.c
 * @brief 滤波器抽象基类实现。
 * @ingroup control
 */

#include "filter_base.h"
#include <stddef.h>

filter_status_t Filter_Base_Construct(filter_base_t *filter_h, const filter_config_t *config,
                                      const filter_ops_t *ops) {
  if (filter_h == NULL || config == NULL || config->name == NULL || ops == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }

  if (ops->init != NULL) {
    filter_status_t status = ops->init(filter_h, config);
    if (status != FILTER_STATUS_OK) {
      return status;
    }
  }

  filter_h->name = config->name;
  filter_h->ops = ops;

  if (ops->reset != NULL) {
    ops->reset(filter_h);
  }
  return FILTER_STATUS_OK;
}

void Filter_Base_Update(filter_base_t *filter_h, const filter_sample_t *input,
                        filter_sample_t *output) {
  if (filter_h == NULL || filter_h->ops == NULL || input == NULL || output == NULL ||
      input == output) {
    return;
  }

  *output = *input;
  Filter_Sample_Sanitize(output);
  if (filter_h->ops->update != NULL) {
    filter_h->ops->update(filter_h, output);
  }
  Filter_Sample_Sanitize(output);
  output->time_ms = input->time_ms;
}

void Filter_Base_Reset(filter_base_t *filter_h) {
  if (filter_h == NULL || filter_h->ops == NULL) {
    return;
  }
  if (filter_h->ops->reset != NULL) {
    filter_h->ops->reset(filter_h);
  }
}
