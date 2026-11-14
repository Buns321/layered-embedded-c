//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file filter_sample.h
 * @brief 固定容量多通道滤波样本。
 * @addtogroup filter
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_FILTER_SAMPLE_H
#define LAYERED_EMBEDDED_C_FILTER_SAMPLE_H

#include "system_check.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief 滤波器最大通道数。 */
#ifndef FILTER_CHANNEL_CAPACITY
#define FILTER_CHANNEL_CAPACITY 16u
#endif
_Static_assert(FILTER_CHANNEL_CAPACITY > 0u, "FILTER_CHANNEL_CAPACITY must be positive");
_Static_assert(FILTER_CHANNEL_CAPACITY <= 32u, "FILTER_CHANNEL_CAPACITY must not exceed 32");
/** @brief 通道有效位掩码。 */
#if FILTER_CHANNEL_CAPACITY == 32u
#define FILTER_CHANNEL_MASK UINT32_MAX
#else
#define FILTER_CHANNEL_MASK \
  ((uint32_t)((1u << FILTER_CHANNEL_CAPACITY) - 1u))
#endif

/**
 * @brief 一次滤波采样。
 * @note value 的下标与 valid_mask 的 bit 一一对应。
 * @note time_ms 是单调毫秒时间戳。
 * @warning valid_mask 中超出 FILTER_CHANNEL_CAPACITY 的位必须为 0。
 */
typedef struct filter_sample_s {
  float    value[FILTER_CHANNEL_CAPACITY];
  uint32_t valid_mask;
  uint32_t time_ms;
} filter_sample_t;

/**
 * @brief 获取指定通道的位掩码。
 * @param channel 通道下标，范围 0..FILTER_CHANNEL_CAPACITY-1。
 * @return 对应通道的单个有效位掩码；返回值不修改样本或共享状态。
 * @pre channel 小于 FILTER_CHANNEL_CAPACITY。
 * @post 不修改任何对象；Debug 构建会断言非法通道。
 * @note 纯查询函数，不分配动态内存。
 */
static inline uint32_t Filter_Sample_GetChannelMask(uint8_t channel) {
  assert(channel < FILTER_CHANNEL_CAPACITY);
  return (uint32_t)1u << channel;
}

/**
 * @brief 查询指定通道在样本中是否有效。
 * @param sample 只读样本；允许为 NULL，此时固定返回 false。
 * @param channel 通道下标，范围 0..FILTER_CHANNEL_CAPACITY-1。
 * @retval true sample->valid_mask 已置位该通道。
 * @retval false 该通道无效或 sample 为空。
 * @pre sample 非空时在调用期间可读。
 * @post 不修改样本；Debug 构建会断言非法通道。
 * @note 纯查询函数，不检查对应通道值是否有限。
 */
static inline bool Filter_Sample_ChannelIsValid(
  const filter_sample_t *sample,
  uint8_t channel
) {
  return sample != NULL &&
         (sample->valid_mask & Filter_Sample_GetChannelMask(channel)) != 0u;
}

/**
 * @brief 清除指定通道的有效位。
 * @param sample 可写样本，不能为 NULL。
 * @param channel 通道下标，范围 0..FILTER_CHANNEL_CAPACITY-1。
 * @pre sample 在调用期间可写；Debug 构建要求 channel 合法。
 * @post sample->valid_mask 中对应通道位被清零，其他通道位和数值不变。
 * @note 不修改通道值，不分配动态内存；并发写入须由调用方串行化。
 */
static inline void Filter_Sample_ClearChannel(
  filter_sample_t *sample,
  uint8_t channel
) {
  assert(sample);
  sample->valid_mask &= ~Filter_Sample_GetChannelMask(channel);
}

/**
 * @brief 仅在结果有限时写入通道值和有效位。
 * @param sample 可写样本，不能为 NULL。
 * @param channel 通道下标，范围 0..FILTER_CHANNEL_CAPACITY-1。
 * @param value 待写入的通道值；单位由具体滤波器的通道 schema 决定。
 * @retval true value 有限，已写入并通过 valid_mask 标记有效。
 * @retval false value 为 NaN/Inf，对应有效位已清除且通道值保持不变。
 * @pre sample 在调用期间可写；Debug 构建要求 channel 合法。
 * @post 有限结果写入通道并置位；非有限结果清除对应有效位。
 * @note 纯本地状态更新，不转义 value 的物理单位，不分配动态内存。
 */
static inline bool Filter_Sample_WriteFiniteChannel(
  filter_sample_t *sample,
  uint8_t channel,
  float value
) {
  uint32_t channel_mask = Filter_Sample_GetChannelMask(channel);

  assert(sample);
  if (!SYS_ISFINITE(value)) {
    sample->valid_mask &= ~channel_mask;
    return false;
  }

  sample->value[channel] = value;
  sample->valid_mask |= channel_mask;
  return true;
}

/**
 * @brief 清理样本中的非法掩码位和非有限有效通道。
 * @param sample 可写样本，不能为 NULL。
 * @pre sample 在调用期间可写且未被并发访问。
 * @post 超出 FILTER_CHANNEL_CAPACITY 的掩码位被清除，非有限值通道的有效位被清除；
 *       通道数值、有效通道和 time_ms 的其他部分保持不变。
 * @note 不分配动态内存；调用方负责串行化同一实例。
 */
static inline void Filter_Sample_Sanitize(filter_sample_t *sample) {
  assert(sample);

  sample->valid_mask &= FILTER_CHANNEL_MASK;
  for (uint8_t channel = 0u; channel < FILTER_CHANNEL_CAPACITY; channel++) {
    uint32_t channel_mask = Filter_Sample_GetChannelMask(channel);
    if (
      (sample->valid_mask & channel_mask) != 0u &&
      !SYS_ISFINITE(sample->value[channel])
    ) {
      sample->valid_mask &= ~channel_mask;
    }
  }
}

/** @} */

#endif //LAYERED_EMBEDDED_C_FILTER_SAMPLE_H
