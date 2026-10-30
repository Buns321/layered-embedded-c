//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file filter_base.h
 * @brief 滤波器抽象接口与预复制语义。
 * @addtogroup filter
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_FILTER_BASE_H
#define LAYERED_EMBEDDED_C_FILTER_BASE_H

#include "filter_sample.h"

typedef struct filter_base_s filter_base_t;

/**
 * @brief 滤波器操作表。
 * @note update 接收只读 input 和调用方拥有的 output；output 已预复制 input，
 *       update 不得改变 time_ms；reset 清除实现内部状态。
 */
typedef struct filter_ops_s {
  void (*update)(filter_base_t *filter_h, const filter_sample_t *input, filter_sample_t *output);
  void (*reset)(filter_base_t *filter_h);
} filter_ops_t;

/**
 * @brief 滤波器抽象对象。
 */
typedef struct filter_base_s {
  const filter_ops_t *ops;
  const char         *name;
} filter_base_t;

/**
 * @brief 初始化滤波器抽象层并绑定操作表。
 * @param filter_h 待初始化的滤波器基对象存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；生命周期必须覆盖滤波器对象。
 * @param ops 非空操作表；update 和 reset 必须有效，且其生命周期覆盖滤波器对象。
 * @pre filter_h 可写；name 和 ops 在滤波器对象存活期间保持有效。
 * @post filter_h->name 与 filter_h->ops 已绑定；不分配或释放资源。
 * @note 仅在初始化阶段调用，不得与同一实例的 update 或 reset 并发。
 */
void Filter_Base_Init(filter_base_t *filter_h, const char *name, const filter_ops_t *ops);
/**
 * @brief 运行一次滤波。
 * @param filter_h 已初始化的滤波器对象，不能为 NULL。
 * @param input 只读输入样本；valid_mask 只能使用 FILTER_CHANNEL_CAPACITY 以内的位。
 * @param output 调用方拥有的输出样本，不能与 input 别名。
 * @pre filter_h 已绑定有效 update 回调；input 与 output 在调用期间可访问。
 * @post output 先完整复制 input，再由具体滤波器覆盖；time_ms 必须保持输入值。
 * @note 输出通道位必须仍是合法掩码；同一实例单写者调用，不分配动态内存。
 */
void Filter_Base_Update(filter_base_t *filter_h, const filter_sample_t *input, filter_sample_t *output);
/**
 * @brief 复位滤波器内部状态。
 * @param filter_h 已初始化的滤波器对象，不能为 NULL。
 * @pre filter_h 已绑定有效 reset 回调，且当前没有并发 update。
 * @post 具体滤波器内部状态被清空；名称、ops 和静态配置保持不变。
 * @note 同一实例单写者调用；不分配动态内存，不改变滤波器拓扑。
 */
void Filter_Base_Reset(filter_base_t *filter_h);

/** @} */

#endif //LAYERED_EMBEDDED_C_FILTER_BASE_H
