/**
 * @file filter_chain.h
 * @brief 固定容量串联滤波器链。
 * @addtogroup control
 * @{
 */

#ifndef LAYERED_CAR_FILTER_CHAIN_H
#define LAYERED_CAR_FILTER_CHAIN_H

#include "filter_base.h"
#include <stdint.h>

#define FILTER_CHAIN_CAPACITY 8U

/**
 * @brief 串联滤波器链静态对象（base 必须为首成员，stages 非拥有指针）。
 */
typedef struct filter_chain_s {
  filter_base_t base;
  filter_base_t *stages[FILTER_CHAIN_CAPACITY];
  uint8_t stage_count;
} filter_chain_t;

/**
 * @brief 初始化串联滤波器链（stage_count 须在 1..FILTER_CHAIN_CAPACITY 且无重复/自引用）。
 */
filter_status_t Filter_Chain_Init(filter_chain_t *filter_h, const char *name,
                                  filter_base_t *const *stages, uint8_t stage_count);

/** @} */

#endif /* LAYERED_CAR_FILTER_CHAIN_H */
