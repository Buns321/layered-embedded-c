/**
 * @file filter_sample.h
 * @brief 固定容量多通道滤波样本结构与安全读写辅助接口。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，完全静态分配。
 *          通过定长浮点数组与 32 位有效位掩码表示多通道物理量样本，所有写入与
 *          清洗操作均强制执行有限值校验（SYS_ISFINITE），防止 NaN/Inf 污染滤波链。
 *
 * @addtogroup Framework_Control
 * @{
 */

#ifndef LAYERED_CAR_FILTER_SAMPLE_H
#define LAYERED_CAR_FILTER_SAMPLE_H

#include "sys_math.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief 滤波器最大通道数（无量纲，编译期上限为 32）。 */
#ifndef FILTER_CHANNEL_CAPACITY
#define FILTER_CHANNEL_CAPACITY 16U
#endif

_Static_assert(FILTER_CHANNEL_CAPACITY > 0U, "FILTER_CHANNEL_CAPACITY must be positive");
_Static_assert(FILTER_CHANNEL_CAPACITY <= 32U, "FILTER_CHANNEL_CAPACITY must not exceed 32");

/** @brief 覆盖全部有效通道的 32 位位掩码常量。 */
#if FILTER_CHANNEL_CAPACITY == 32U
#define FILTER_CHANNEL_MASK UINT32_MAX
#else
#define FILTER_CHANNEL_MASK ((uint32_t)((1UL << FILTER_CHANNEL_CAPACITY) - 1UL))
#endif

/**
 * @brief 多通道滤波样本（value 下标与 valid_mask 位号对应，time_ms 单位 ms）。
 */
typedef struct filter_sample_s {
  float value[FILTER_CHANNEL_CAPACITY]; /**< 各通道物理量采样值数组（单位由上游传感器定义，如
                                           m/s^2、deg/s、deg）。 */
  uint32_t valid_mask; /**< 通道有效位掩码；bit[i]==1 表示 value[i] 有效且为有限值。 */
  uint32_t time_ms;    /**< 采样单调时间戳（单位：ms）。 */
} filter_sample_t;

/**
 * @brief 获取通道位掩码；channel 越界时返回 0。
 * @param[in] channel 通道索引（范围：0..FILTER_CHANNEL_CAPACITY-1）。
 * @return 对应的 32 位通道掩码；越界时返回 0U。
 */
static inline uint32_t Filter_Sample_GetChannelMask(uint8_t channel) {
  if (channel >= FILTER_CHANNEL_CAPACITY) {
    return 0U;
  }
  return (uint32_t)1U << channel;
}

/**
 * @brief 查询通道在样本中是否标记为有效。
 * @param[in] sample  样本结构体指针（允许为 NULL，此时返回 false）。
 * @param[in] channel 通道索引（范围：0..FILTER_CHANNEL_CAPACITY-1）。
 * @return 通道是否有效。
 * @retval true  sample 非 NULL、通道未越界且对应 valid_mask 位为 1。
 * @retval false sample 为 NULL、通道越界或对应有效位为 0。
 */
static inline bool Filter_Sample_ChannelIsValid(const filter_sample_t *sample, uint8_t channel) {
  uint32_t mask = Filter_Sample_GetChannelMask(channel);
  return sample != NULL && mask != 0U && (sample->valid_mask & mask) != 0U;
}

/**
 * @brief 清除指定通道的有效位（不修改 value 数组元素）。
 * @param[in,out] sample  样本结构体指针（为 NULL 时静默返回）。
 * @param[in]     channel 通道索引（越界时掩码为 0，无副作用）。
 */
static inline void Filter_Sample_ClearChannel(filter_sample_t *sample, uint8_t channel) {
  if (sample == NULL) {
    return;
  }
  sample->valid_mask &= ~Filter_Sample_GetChannelMask(channel);
}

/**
 * @brief 仅当 value 为有限值时写入通道并置有效位；非有限值则清除有效位并返回 false。
 * @param[in,out] sample  样本结构体指针（为 NULL 时返回 false）。
 * @param[in]     channel 通道索引（范围：0..FILTER_CHANNEL_CAPACITY-1）。
 * @param[in]     value   待写入的通道物理量（须为有限浮点数）。
 * @return 写入是否成功。
 * @retval true  value 为有限数，已写入 value[channel] 并置位 valid_mask。
 * @retval false sample 为 NULL、通道越界或 value 为 NaN/Inf（已清除有效位）。
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
 * @param[in,out] sample 样本结构体指针（为 NULL 时静默返回）。
 * @note 仅修改 valid_mask，不覆盖 value 数组中的原始数值。
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

#endif /* LAYERED_CAR_FILTER_SAMPLE_H */

/** @} */
