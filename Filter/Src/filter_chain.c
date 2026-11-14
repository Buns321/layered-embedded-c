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

typedef struct filter_chain_init_data_s {
  filter_base_t *const *filter_stage_h;
  uint8_t stage_count;
} filter_chain_init_data_t;

/**
 * @brief 校验并初始化 chain 具体配置。
 * @param filter_h 待构造的 chain 对象；用于拒绝自引用，首次写入前完成全部校验。
 * @param filter_config 非空基配置；data 指向 filter_chain_init_data_t。
 * @retval FILTER_STATUS_OK 级联表和级数有效。
 * @retval FILTER_STATUS_INVALID_ARGUMENT 级联表为空。
 * @retval FILTER_STATUS_INVALID_CONFIG 级数、自引用、空 stage、无效回调或重复项非法。
 * @pre filter_config 指向完整的 chain 私有配置。
 * @post 成功时全部 stage 槽位和级数已写入；失败时不修改链对象。
 * @note 私有构造回调，不分配动态内存。
 */
static filter_status_t filter_chain_init(
  filter_base_t *filter_h,
  const filter_config_t *filter_config
) {
  filter_chain_t *filter_chain_h = (filter_chain_t *)filter_h;
  const filter_chain_init_data_t *init_data =
    (const filter_chain_init_data_t *)filter_config->data;

  if (init_data == NULL || init_data->filter_stage_h == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }
  if (
    init_data->stage_count == 0u ||
    init_data->stage_count > FILTER_CHAIN_CAPACITY
  ) {
    return FILTER_STATUS_INVALID_CONFIG;
  }

  for (uint8_t i = 0; i < init_data->stage_count; i++) {
    filter_base_t *stage_h = init_data->filter_stage_h[i];
    if (
      stage_h == NULL ||
      stage_h == filter_h ||
      stage_h->ops == NULL
    ) {
      return FILTER_STATUS_INVALID_CONFIG;
    }

    for (uint8_t j = 0; j < i; j++) {
      if (stage_h == init_data->filter_stage_h[j]) {
        return FILTER_STATUS_INVALID_CONFIG;
      }
    }
  }

  for (uint8_t i = 0; i < FILTER_CHAIN_CAPACITY; i++) {
    filter_chain_h->filter_stage_h[i] =
      i < init_data->stage_count ? init_data->filter_stage_h[i] : NULL;
  }
  filter_chain_h->stage_count = init_data->stage_count;
  return FILTER_STATUS_OK;
}

/**
 * @brief 重置链条中的全部滤波器。
 * @param filter_h 已初始化的滤波器链基对象，不能为 NULL。
 * @pre stage_count 在 1..FILTER_CHAIN_CAPACITY 内，且全部 stage 指针有效。
 * @post 每个 stage 的 reset 均被调用；链自身名称、ops 和级联表不变。
 * @note 按级联顺序同步执行，非线程安全；不分配动态内存。
 */
static void filter_chain_reset(filter_base_t *filter_h) {
  filter_chain_t *filter_chain_h = (filter_chain_t *)filter_h;

  for (uint8_t i = 0; i < filter_chain_h->stage_count; i++) {
    Filter_Base_Reset(filter_chain_h->filter_stage_h[i]);
  }
}

/**
 * @brief 依次执行链条中的滤波器。
 * @param filter_h 已初始化的滤波器链基对象，不能为 NULL。
 * @param sample base 已预复制并清洗的样本；链中第一级接收该样本。
 * @pre stage_count 和全部 stage 指针已通过 Init 校验，sample 可读写。
 * @post 每级输出作为下一级输入，最终样本复制回 sample；time_ms 保持输入值。
 * @note 使用两个栈上 filter_sample_t 作为交替中间缓冲；不分配动态内存，单写者调用。
 */
static void filter_chain_update(
  filter_base_t *filter_h,
  filter_sample_t *sample
) {
  filter_chain_t *filter_chain_h = (filter_chain_t *)filter_h;

  filter_sample_t scratch_a;
  filter_sample_t scratch_b;
  const filter_sample_t *stage_input = sample;

  for (uint8_t i = 0; i < filter_chain_h->stage_count; i++) {
    filter_sample_t *stage_output =
      ((i & 1u) == 0u) ? &scratch_a : &scratch_b;

    Filter_Base_Update(filter_chain_h->filter_stage_h[i], stage_input, stage_output);
    stage_input = stage_output;
  }

  *sample = *stage_input;
}

static const filter_ops_t filter_ops_chain = {
  .init = filter_chain_init,
  .update = filter_chain_update,
  .reset = filter_chain_reset,
};

/**
 * @brief 初始化滤波器链。
 * @param filter_h 待初始化的滤波器链存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；生命周期必须覆盖链对象。
 * @param filter_stage_h 非空级联表；前 stage_count 项必须非空、已初始化且互不重复。
 * @param stage_count 级数，范围 1..FILTER_CHAIN_CAPACITY。
 * @retval FILTER_STATUS_OK 级联表有效，链已完成初始化。
 * @retval FILTER_STATUS_INVALID_ARGUMENT filter_h、name 或级联表为空。
 * @retval FILTER_STATUS_INVALID_CONFIG 级数、自引用、空 stage、无效回调或重复项非法。
 * @pre 任一 stage 不能指向 filter_h 自身的基对象；表项生命周期必须覆盖链对象。
 * @post 级联表复制入对象、剩余槽位清零，并按顺序 reset 全部 stage。
 * @post 失败时对象保持不变。
 * @note 仅允许初始化阶段单线程调用。
 */
filter_status_t Filter_Chain_Init(
  filter_chain_t *filter_h,
  const char *name,
  filter_base_t *const *filter_stage_h,
  uint8_t stage_count
) {
  if (filter_h == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }

  const filter_chain_init_data_t init_data = {
    .filter_stage_h = filter_stage_h,
    .stage_count = stage_count,
  };
  const filter_config_t config = {
    .name = name,
    .data = &init_data,
  };
  return Filter_Base_Construct(
    &filter_h->filter_base,
    &config,
    &filter_ops_chain
  );
}
