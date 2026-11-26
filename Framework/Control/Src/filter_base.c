/**
 * @file filter_base.c
 * @brief 滤波器抽象基类实现。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，完全静态分配。
 *          实现通用构造器（先派生校验后绑定虚表再复位）及带前后双向有限值清洗的
 *          安全更新流水线。
 * @ingroup Framework_Control
 */

#include "filter_base.h"
#include <stddef.h>

filter_status_t Filter_Base_Construct(filter_base_t *filter_h, const filter_config_t *config,
                                      const filter_ops_t *ops) {
  if (filter_h == NULL || config == NULL || config->name == NULL || ops == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }

  // 先执行派生初始化与参数校验；失败时直接返回错误码，确保 filter_h 原有绑定不被破坏
  if (ops->init != NULL) {
    filter_status_t status = ops->init(filter_h, config);
    if (status != FILTER_STATUS_OK) {
      return status;
    }
  }

  filter_h->name = config->name;
  filter_h->ops = ops;

  // 构造完成后自动复位一次内部状态，保证首帧滤波从干净零态启动
  if (ops->reset != NULL) {
    ops->reset(filter_h);
  }
  return FILTER_STATUS_OK;
}

void Filter_Base_Update(filter_base_t *filter_h, const filter_sample_t *input,
                        filter_sample_t *output) {
  // 禁止 input 与 output 指针混叠，避免读写交错或损坏原始时间戳
  if (filter_h == NULL || filter_h->ops == NULL || input == NULL || output == NULL ||
      input == output) {
    return;
  }

  // 预复制 + 前置清洗：剔除越界通道位及非有限数值，保护派生滤波器不受 NaN/Inf 污染
  *output = *input;
  Filter_Sample_Sanitize(output);
  if (filter_h->ops->update != NULL) {
    filter_h->ops->update(filter_h, output);
  }
  // 后置清洗并强制恢复输入时间戳，确保滤波器不会意外篡改样本时间基准
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
