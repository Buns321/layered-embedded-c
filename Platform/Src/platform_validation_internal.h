//
// 由 Buns_ 创建于 2026/10/23。
//

/**
 * @file platform_validation_internal.h
 * @brief 与原生测试共用的纯校验与算术辅助函数。
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_VALIDATION_INTERNAL_H
#define LAYERED_EMBEDDED_C_PLATFORM_VALIDATION_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PLATFORM_SECONDS_PER_DAY 86400UL

static inline bool platform_can_identifier_is_valid(
  bool extended,
  uint32_t identifier
) {
  return extended
           ? identifier <= 0x1FFFFFFFUL
           : identifier <= 0x7FFUL;
}

static inline bool platform_can_frame_values_are_valid(
  bool extended,
  uint32_t identifier,
  uint8_t dlc
) {
  return dlc <= 8U &&
         platform_can_identifier_is_valid(extended, identifier);
}

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

static inline uint32_t platform_rtc_next_daily_alarm(
  uint32_t alarm_counter
) {
  /* 32 位 RTC 计数器到达范围末尾后会自然回绕。 */
  return alarm_counter + PLATFORM_SECONDS_PER_DAY;
}

#endif //LAYERED_EMBEDDED_C_PLATFORM_VALIDATION_INTERNAL_H
