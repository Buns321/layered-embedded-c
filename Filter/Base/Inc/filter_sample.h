//
// Created by Buns_ on 2026/10/20.
//

/**
 * @file filter_sample.h
 * @brief 固定容量多通道滤波样本。
 * @addtogroup filter
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_FILTER_SAMPLE_H
#define LAYERED_EMBEDDED_C_FILTER_SAMPLE_H

#include <stdint.h>

/** @brief 滤波器最大通道数。 */
#define FILTER_CHANNEL_CAPACITY 16u
_Static_assert(FILTER_CHANNEL_CAPACITY > 0u, "FILTER_CHANNEL_CAPACITY must be positive");
_Static_assert(FILTER_CHANNEL_CAPACITY <= 32u, "FILTER_CHANNEL_CAPACITY must not exceed 32");
/** @brief 通道有效位掩码。 */
#define FILTER_CHANNEL_MASK \
  ((uint32_t)((1u << FILTER_CHANNEL_CAPACITY) - 1u))

/**
 * @brief 一次滤波采样。
 * @note value 的下标与 valid_mask 的 bit 一一对应。
 * @note time_ms 是单调毫秒时间戳。
 * @warning valid_mask 中超出 FILTER_CHANNEL_CAPACITY 的位必须为 0。
 */
typedef struct {
  float    value[FILTER_CHANNEL_CAPACITY];
  uint32_t valid_mask;
  uint32_t time_ms;
} filter_sample_t;

/** @} */

#endif //LAYERED_EMBEDDED_C_FILTER_SAMPLE_H
