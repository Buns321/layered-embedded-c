//
// Created by Buns_ on 2026/10/20.
//

/**
 * @file filter_chain.h
 * @brief 串联滤波器链。
 * @addtogroup filter
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_FILTER_CHAIN_H
#define LAYERED_EMBEDDED_C_FILTER_CHAIN_H

#include "filter_base.h"
#include <stdint.h>

/** @brief 滤波器链最大级数。 */
#define FILTER_CHAIN_CAPACITY 8u

/**
 * @brief 串联滤波器链对象。
 * @note 每一级的输出作为下一级的输入；级数固定，不使用动态内存。
 */
typedef struct filter_chain {
  filter_base_t  filter_base;
  filter_base_t *stages[FILTER_CHAIN_CAPACITY];
  uint8_t        stage_count;
} filter_chain_t;

/**
 * @brief 初始化滤波器链。
 * @param this 对象。
 * @param name 名称。
 * @param stages 静态级联表。
 * @param stage_count 级数，范围 1..FILTER_CHAIN_CAPACITY。
 * @note Init 会校验空指针、自引用和重复项，并级联 reset。
 */
void Filter_Chain_Init(filter_chain_t *this, const char *name, filter_base_t *const *stages, uint8_t stage_count);

/** @} */

#endif //LAYERED_EMBEDDED_C_FILTER_CHAIN_H
