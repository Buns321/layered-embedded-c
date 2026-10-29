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

/** @brief 初始化滤波器抽象层并绑定操作表。 */
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

/**
 * @brief 运行一次滤波。
 * @param filter_h 滤波器对象。
 * @param input 输入样本。
 * @param output 输出样本，不能与 input 别名。
 * @note 调用前会把 input 全量复制到 output，具体滤波器再覆盖或增加通道。
 * @note time_ms 必须保持不变。
 */
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

/** @brief 复位滤波器内部状态。 */
void Filter_Base_Reset(filter_base_t *filter_h) {
  assert(filter_h && filter_h->ops && filter_h->ops->reset);
  filter_h->ops->reset(filter_h);
}
