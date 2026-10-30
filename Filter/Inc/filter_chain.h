//
// 由 Buns_ 创建于 2026/10/20。
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
typedef struct filter_chain_s {
  filter_base_t  filter_base;
  filter_base_t *filter_stage_h[FILTER_CHAIN_CAPACITY];
  uint8_t        stage_count;
} filter_chain_t;

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
void Filter_Chain_Init(filter_chain_t *filter_h, const char *name, filter_base_t *const *filter_stage_h, uint8_t stage_count);

/** @} */

#endif //LAYERED_EMBEDDED_C_FILTER_CHAIN_H
