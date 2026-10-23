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

typedef struct filter_base filter_base_t;

/**
 * @brief 滤波器操作表。
 */
typedef struct filter_ops {
  void (*update)(filter_base_t *this, const filter_sample_t *input, filter_sample_t *output);
  void (*reset)(filter_base_t *this);
} filter_ops_t;

/**
 * @brief 滤波器抽象对象。
 */
typedef struct filter_base {
  const filter_ops_t *ops;
  const char         *name;
} filter_base_t;

/** @brief 初始化滤波器抽象层并绑定操作表。 */
void Filter_Base_Init(filter_base_t *this, const char *name, const filter_ops_t *ops);
/**
 * @brief 运行一次滤波。
 * @param this 滤波器对象。
 * @param input 输入样本。
 * @param output 输出样本，不能与 input 别名。
 * @note 调用前会把 input 全量复制到 output，具体滤波器再覆盖或增加通道。
 * @note time_ms 必须保持不变。
 */
void Filter_Base_Update(filter_base_t *this, const filter_sample_t *input, filter_sample_t *output);
/** @brief 复位滤波器内部状态。 */
void Filter_Base_Reset(filter_base_t *this);

/** @} */

#endif //LAYERED_EMBEDDED_C_FILTER_BASE_H
