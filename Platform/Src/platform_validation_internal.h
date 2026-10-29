//
// 由 Buns_ 创建于 2026/10/23。
//

/**
 * @file platform_validation_internal.h
 * @brief 与原生测试共用的纯校验与算术辅助函数。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_VALIDATION_INTERNAL_H
#define LAYERED_EMBEDDED_C_PLATFORM_VALIDATION_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PLATFORM_SECONDS_PER_DAY 86400UL

/**
 * @brief 校验经典 CAN 标识符范围。
 * @param extended 是否使用扩展帧。
 * @param identifier 标识符位值。
 * @return 标识符在对应范围内时返回 true。
 */
static inline bool platform_can_identifier_is_valid(
  bool extended,
  uint32_t identifier
) {
  return extended
           ? identifier <= 0x1FFFFFFFUL
           : identifier <= 0x7FFUL;
}

/**
 * @brief 校验经典 CAN 标识符和数据长度。
 * @param extended 是否使用扩展帧。
 * @param identifier 标识符位值。
 * @param dlc 数据长度，范围 0..8。
 * @return 标识符和 DLC 都合法时返回 true。
 */
static inline bool platform_can_frame_values_are_valid(
  bool extended,
  uint32_t identifier,
  uint8_t dlc
) {
  return dlc <= 8U &&
         platform_can_identifier_is_valid(extended, identifier);
}

/**
 * @brief 校验 SPI 传输缓冲区和长度约束。
 * @param data 传输缓冲区；不能为空。
 * @param size 字节数；必须大于 0。
 * @param data_width_16 是否使用 16 位数据宽度。
 * @return 缓冲区、长度和对齐要求都满足时返回 true。
 */
static inline bool platform_spi_transfer_values_are_valid(
  const void *data,
  uint16_t size,
  bool data_width_16
) {
  if (data == NULL || size == 0U) {
    return false;
  }
  if (!data_width_16) {
    return true;
  }
  return (size & 1U) == 0U &&
         (((uintptr_t)data & 1U) == 0U);
}

/**
 * @brief 计算下一天的 RTC 闹钟计数器值。
 * @param alarm_counter 当前闹钟计数器值。
 * @return 增加一天秒数后的计数器值；32 位回绕由调用方按 RTC 语义处理。
 */
static inline uint32_t platform_rtc_next_daily_alarm(
  uint32_t alarm_counter
) {
  /* 32 位 RTC 计数器到达范围末尾后会自然回绕。 */
  return alarm_counter + PLATFORM_SECONDS_PER_DAY;
}

#endif //LAYERED_EMBEDDED_C_PLATFORM_VALIDATION_INTERNAL_H
