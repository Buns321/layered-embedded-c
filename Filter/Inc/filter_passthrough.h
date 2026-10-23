//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file filter_passthrough.h
 * @brief 透传滤波器。
 * @addtogroup filter
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_FILTER_PASSTHROUGH_H
#define LAYERED_EMBEDDED_C_FILTER_PASSTHROUGH_H

#include "filter_base.h"

/**
 * @brief 透传滤波器对象。
 * @note 依赖 Filter_Base_Update 的预复制语义，直接输出原始通道。
 */
typedef struct filter_passthrough {
  filter_base_t filter_base;
} filter_passthrough_t;

/**
 * @brief 初始化透传滤波器。
 * @param this 对象。
 * @param name 名称。
 */
void Filter_Passthrough_Init(
  filter_passthrough_t *this,
  const char *name
);

/** @} */

#endif //LAYERED_EMBEDDED_C_FILTER_PASSTHROUGH_H
