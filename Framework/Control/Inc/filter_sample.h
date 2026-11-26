/**
 * @file filter_sample.h
 * @brief 固定容量多通道滤波样本结构与安全读写辅助接口。
 * @addtogroup control
 * @{
 */

#ifndef LAYERED_CAR_FILTER_SAMPLE_H
#define LAYERED_CAR_FILTER_SAMPLE_H

#include "sys_math.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief 滤波器最大通道数。 */
#ifndef FILTER_CHANNEL_CAPACITY
#define FILTER_CHANNEL_CAPACITY 16U
#endif

_Static_assert(FILTER_CHANNEL_CAPACITY > 0U, "FILTER_CHANNEL_CAPACITY must be positive");
_Static_assert(FILTER_CHANNEL_CAPACITY <= 32U, "FILTER_CHANNEL_CAPACITY must not exceed 32");

#if FILTER_CHANNEL_CAPACITY == 32U
#define FILTER_CHANNEL_MASK UINT32_MAX
#else
#define FILTER_CHANNEL_MASK ((uint32_t)((1UL << FILTER_CHANNEL_CAPACITY) - 1UL))
#endif

/**
 * @brief 多通道滤波样本（value 下标与 valid_mask 位号对应，time_ms 单位 ms）。
 */
typedef struct filter_sample_s {
  float value[FILTER_CHANNEL_CAPACITY];
  uint32_t valid_mask;
  uint32_t time_ms;
} filter_sample_t;

/**
 * @brief 获取通道位掩码；channel 越界时返回 0。
 */
static inline uint32_t Filter_Sample_GetChannelMask(uint8_t channel) {
  if (channel >= FILTER_CHANNEL_CAPACITY) {
    return 0U;
  }
  return (uint32_t)1U << channel;
}

/**
 * @brief 查询通道在样本中是否标记为有效。
 */
static inline bool Filter_Sample_ChannelIsValid(const filter_sample_t *sample, uint8_t channel) {
  uint32_t mask = Filter_Sample_GetChannelMask(channel);
  return sample != NULL && mask != 0U && (sample->valid_mask & mask) != 0U;
}

/**
 * @brief 清除指定通道的有效位（不修改 value 数组元素）。
 */
static inline void Filter_Sample_ClearChannel(filter_sample_t *sample, uint8_t channel) {
  if (sample == NULL) {
    return;
  }
  sample->valid_mask &= ~Filter_Sample_GetChannelMask(channel);
}

/**
 * @brief 仅当 value 为有限值时写入通道并置有效位；非有限值则清除有效位并返回 false。
 */
static inline bool Filter_Sample_WriteFiniteChannel(filter_sample_t *sample, uint8_t channel,
                                                    float value) {
  uint32_t mask = Filter_Sample_GetChannelMask(channel);
  if (sample == NULL || mask == 0U) {
    return false;
  }
  if (!SYS_ISFINITE(value)) {
    sample->valid_mask &= ~mask;
    return false;
  }
  sample->value[channel] = value;
  sample->valid_mask |= mask;
  return true;
}

/**
 * @brief 清除超出容量的掩码位及值为非有限数的通道有效位。
 */
static inline void Filter_Sample_Sanitize(filter_sample_t *sample) {
  if (sample == NULL) {
    return;
  }
  sample->valid_mask &= FILTER_CHANNEL_MASK;
  for (uint8_t channel = 0U; channel < FILTER_CHANNEL_CAPACITY; ++channel) {
    uint32_t mask = Filter_Sample_GetChannelMask(channel);
    if ((sample->valid_mask & mask) != 0U && !SYS_ISFINITE(sample->value[channel])) {
      sample->valid_mask &= ~mask;
    }
  }
}

/** @} */

#endif /* LAYERED_CAR_FILTER_SAMPLE_H */
