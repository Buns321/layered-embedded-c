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
#include <assert.h>

/**
 * @brief 重置链条中的全部滤波器。
 * @param filter_h 已初始化的滤波器链基对象，不能为 NULL。
 * @pre stage_count 在 1..FILTER_CHAIN_CAPACITY 内，且全部 stage 指针有效。
 * @post 每个 stage 的 reset 均被调用；链自身名称、ops 和级联表不变。
 * @note 按级联顺序同步执行，非线程安全；不分配动态内存。
 */
static void filter_chain_reset(filter_base_t *filter_h) {
  assert(filter_h);
  filter_chain_t *filter_chain_h = (filter_chain_t *)filter_h;

  assert(filter_chain_h->stage_count > 0u);
  assert(filter_chain_h->stage_count <= FILTER_CHAIN_CAPACITY);

  for (uint8_t i = 0; i < filter_chain_h->stage_count; i++) {
    assert(filter_chain_h->filter_stage_h[i]);
    Filter_Base_Reset(filter_chain_h->filter_stage_h[i]);
  }
}

/**
 * @brief 依次执行链条中的滤波器。
 * @param filter_h 已初始化的滤波器链基对象，不能为 NULL。
 * @param input 只读输入样本；链中第一级接收该样本。
 * @param output 调用方拥有的最终输出，不能与 input 别名。
 * @pre stage_count 和全部 stage 指针有效，output 可写。
 * @post 每级输出作为下一级输入，最终样本复制到 output；time_ms 保持输入值。
 * @note 使用两个栈上 filter_sample_t 作为交替中间缓冲；不分配动态内存，单写者调用。
 */
static void filter_chain_update(filter_base_t *filter_h, const filter_sample_t *input, filter_sample_t *output) {
  assert(filter_h && input && output);
  filter_chain_t *filter_chain_h = (filter_chain_t *)filter_h;

  assert(filter_chain_h->stage_count > 0u);
  assert(filter_chain_h->stage_count <= FILTER_CHAIN_CAPACITY);

  filter_sample_t scratch_a;
  filter_sample_t scratch_b;
  const filter_sample_t *stage_input = input;

  for (uint8_t i = 0; i < filter_chain_h->stage_count; i++) {
    filter_sample_t *stage_output =
      ((i & 1u) == 0u) ? &scratch_a : &scratch_b;

    assert(filter_chain_h->filter_stage_h[i]);
    Filter_Base_Update(filter_chain_h->filter_stage_h[i], stage_input, stage_output);
    stage_input = stage_output;
  }

  *output = *stage_input;
}

static const filter_ops_t filter_ops_chain = {
  .update = filter_chain_update,
  .reset  = filter_chain_reset,
};

/**
 * @brief 初始化滤波器链。
 * @param filter_h 待初始化的滤波器链存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；生命周期必须覆盖链对象。
 * @param filter_stage_h 非空级联表；前 stage_count 项必须非空、已初始化且互不重复。
 * @param stage_count 级数，范围 1..FILTER_CHAIN_CAPACITY。
 * @pre 任一 stage 不能指向 filter_h 自身的基对象；表项生命周期必须覆盖链对象。
 * @post 级联表复制入对象、剩余槽位清零，并按顺序 reset 全部 stage。
 * @note Debug 构建检查自引用和重复项；仅允许初始化阶段单线程调用。
 */
void Filter_Chain_Init(filter_chain_t *filter_h, const char *name, filter_base_t *const *filter_stage_h, uint8_t stage_count) {
  assert(
    filter_h                                 &&
    name                                 &&
    filter_stage_h                               &&
    stage_count > 0u                     &&
    stage_count <= FILTER_CHAIN_CAPACITY
  );

  for (uint8_t i = 0; i < stage_count; i++) {
    assert(
      filter_stage_h[i]                       &&
      filter_stage_h[i] != &filter_h->filter_base &&
      filter_stage_h[i]->ops                  &&
      filter_stage_h[i]->ops->update          &&
      filter_stage_h[i]->ops->reset
    );

    for (uint8_t j = 0; j < i; j++) {
      assert(filter_stage_h[i] != filter_stage_h[j]);
    }
  }

  Filter_Base_Init(&filter_h->filter_base, name, &filter_ops_chain);

  for (uint8_t i = 0; i < FILTER_CHAIN_CAPACITY; i++) {
    filter_h->filter_stage_h[i] = NULL;
  }

  for (uint8_t i = 0; i < stage_count; i++) {
    filter_h->filter_stage_h[i] = filter_stage_h[i];
  }

  filter_h->stage_count = stage_count;

  Filter_Base_Reset(&filter_h->filter_base);
}
