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

/**
 * @brief 拷贝输入样本并维护有效通道。
 * @param filter_h 已初始化的透传滤波器基对象；不读取其内部状态，不能为 NULL。
 * @param input 只读输入样本；基类已将其全量复制到 output。
 * @param output 调用方拥有的输出样本；本函数不再修改。
 * @pre Filter_Base_Update 已完成 input 到 output 的预复制。
 * @post output 与 input 完全相同，包括 value、valid_mask 和 time_ms。
 * @note 无内部状态、无动态内存；之所以保留 update，是为了满足 filter_ops_t 契约。
 */
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

/**
 * @brief 重置透传滤波器状态。
 * @param filter_h 已初始化的透传滤波器基对象，不能为 NULL。
 * @pre 无额外前置条件；透传实现没有运行时状态。
 * @post 对象状态不变，后续 update 仍原样透传输入样本。
 * @note 空操作；不分配动态内存，不改变名称或 ops。
 */
static void filter_passthrough_reset(filter_base_t *filter_h) {
  assert(filter_h);

  (void)filter_h;
}

static const filter_ops_t filter_ops_passthrough = {
  .update = filter_passthrough_update,
  .reset  = filter_passthrough_reset,
};

/**
 * @brief 初始化透传滤波器。
 * @param filter_h 待初始化的透传滤波器存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；生命周期必须覆盖滤波器对象。
 * @pre filter_h 可写；name 在滤波器对象存活期间保持有效。
 * @post 滤波器绑定透传 update/reset 回调；不复制或分配任何通道缓冲。
 * @note 仅在初始化阶段调用；不分配动态内存。
 */
void Filter_Passthrough_Init(
  filter_passthrough_t *filter_h,
  const char *name
) {
  assert(filter_h && name);

  Filter_Base_Init(&filter_h->filter_base, name, &filter_ops_passthrough);
}
