/**
 * @file filter_chain.c
 * @brief 固定容量串联滤波器链实现。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，完全静态分配。
 *          通过栈上双缓冲（scratch_a / scratch_b）交替作为各级输入与输出，满足
 *          Filter_Base_Update() 对 input != output 非混叠的契约要求。
 * @ingroup Framework_Control
 */

#include "filter_chain.h"
#include <stddef.h>

/**
 * @brief 滤波器链内部构造透传参数结构体（仅用于 Init 阶段传递 stages 与级数）。
 */
typedef struct filter_chain_init_data_s {
  filter_base_t *const *stages; /**< 外部传入的子滤波器指针数组。 */
  uint8_t stage_count;          /**< 有效子滤波器级数（1..FILTER_CHAIN_CAPACITY）。 */
} filter_chain_init_data_t;

/**
 * @brief 滤波器链虚表 init 回调：校验子滤波器合法性并绑定数组。
 * @param[in,out] filter_h      滤波器链基类指针（实际类型为 filter_chain_t*）。
 * @param[in]     filter_config 构造器配置（data 指向 filter_chain_init_data_t）。
 * @return 初始化状态码。
 */
static filter_status_t filter_chain_init(filter_base_t *filter_h,
                                         const filter_config_t *filter_config) {
  filter_chain_t *chain = (filter_chain_t *)filter_h;
  const filter_chain_init_data_t *init_data = (const filter_chain_init_data_t *)filter_config->data;

  if (init_data == NULL || init_data->stages == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }
  if (init_data->stage_count == 0U || init_data->stage_count > FILTER_CHAIN_CAPACITY) {
    return FILTER_STATUS_INVALID_CONFIG;
  }

  // 全量预校验：禁止空指针、自引用（防止无限递归）、未构造子级与重复级联同一实例
  for (uint8_t i = 0U; i < init_data->stage_count; ++i) {
    const filter_base_t *stage_h = init_data->stages[i];
    if (stage_h == NULL || stage_h == filter_h || stage_h->ops == NULL) {
      return FILTER_STATUS_INVALID_CONFIG;
    }
    for (uint8_t j = 0U; j < i; ++j) {
      if (stage_h == init_data->stages[j]) {
        return FILTER_STATUS_INVALID_CONFIG;
      }
    }
  }

  for (uint8_t i = 0U; i < FILTER_CHAIN_CAPACITY; ++i) {
    chain->stages[i] = (i < init_data->stage_count) ? init_data->stages[i] : NULL;
  }
  chain->stage_count = init_data->stage_count;
  return FILTER_STATUS_OK;
}

/**
 * @brief 滤波器链虚表 reset 回调：级联复位全部子滤波器。
 * @param[in,out] filter_h 滤波器链基类指针。
 */
static void filter_chain_reset(filter_base_t *filter_h) {
  filter_chain_t *chain = (filter_chain_t *)filter_h;
  for (uint8_t i = 0U; i < chain->stage_count; ++i) {
    Filter_Base_Reset(chain->stages[i]);
  }
}

/**
 * @brief 滤波器链虚表 update 回调：使用双缓冲依次串联执行各级滤波。
 * @param[in,out] filter_h 滤波器链基类指针。
 * @param[in,out] sample   已由 Filter_Base_Update 预清洗的样本指针（原地写回末级结果）。
 */
static void filter_chain_update(filter_base_t *filter_h, filter_sample_t *sample) {
  filter_chain_t *chain = (filter_chain_t *)filter_h;
  filter_sample_t scratch_a;
  filter_sample_t scratch_b;
  const filter_sample_t *stage_input = sample;

  // 乒乓缓冲传递：偶数级输出至 scratch_a，奇数级输出至 scratch_b，保证 input != output
  for (uint8_t i = 0U; i < chain->stage_count; ++i) {
    filter_sample_t *stage_output = ((i & 1U) == 0U) ? &scratch_a : &scratch_b;
    Filter_Base_Update(chain->stages[i], stage_input, stage_output);
    stage_input = stage_output;
  }

  // 将最终级输出写回 sample，外层 Filter_Base_Update 会再次清洗并恢复输入 time_ms
  *sample = *stage_input;
}

/** @brief 串联滤波器链静态只读虚表。 */
static const filter_ops_t s_filter_ops_chain = {
    .init = filter_chain_init,
    .update = filter_chain_update,
    .reset = filter_chain_reset,
};

filter_status_t Filter_Chain_Init(filter_chain_t *filter_h, const char *name,
                                  filter_base_t *const *stages, uint8_t stage_count) {
  if (filter_h == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }
  const filter_chain_init_data_t init_data = {
      .stages = stages,
      .stage_count = stage_count,
  };
  const filter_config_t config = {
      .name = name,
      .data = &init_data,
  };
  return Filter_Base_Construct(&filter_h->base, &config, &s_filter_ops_chain);
}
