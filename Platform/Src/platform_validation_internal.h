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
 * @param extended true 按 29 位扩展帧校验，false 按 11 位标准帧校验。
 * @param identifier 仅含 CAN 标识符位的值，不包含 IDE、RTR 或硬件寄存器标志。
 * @return 标识符不超过所选帧格式上限时返回 true，否则返回 false。
 * @note 纯函数，不访问硬件、不分配内存且无共享状态，可从任务或 ISR 上下文调用。
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
 * @param extended true 按 29 位扩展帧校验，false 按 11 位标准帧校验。
 * @param identifier 仅含 CAN 标识符位的值，不包含 IDE、RTR 或硬件寄存器标志。
 * @param dlc 经典 CAN 数据长度，范围 0..8 字节。
 * @return 标识符范围和 dlc 同时合法时返回 true，否则返回 false。
 * @note 纯函数，不访问硬件、不分配内存且无共享状态，可从任务或 ISR 上下文调用。
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
 * @param data 待传输缓冲区；可为 NULL，由本函数显式拒绝。
 * @param size 传输字节数，不是硬件帧数；必须大于 0。
 * @param data_width_16 true 要求 size 为偶数字节且 data 地址按 2 字节对齐。
 * @return data 非空、size 非零且满足所选数据宽度约束时返回 true，否则返回 false。
 * @note size 与 data_width_16 描述的是同一 CubeMX 配置；纯函数且无共享状态。
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
 * @param alarm_counter 当前 RTC 秒计数器值，单位为秒。
 * @return alarm_counter 加 86400 秒后的值；32 位无符号回绕由 RTC 硬件语义处理。
 * @note 纯算术函数，不读取 RTC 寄存器，也不判断日期或夏令时。
 */
static inline uint32_t platform_rtc_next_daily_alarm(
  uint32_t alarm_counter
) {
  /* 32 位 RTC 计数器到达范围末尾后会自然回绕。 */
  return alarm_counter + PLATFORM_SECONDS_PER_DAY;
}

#endif //LAYERED_EMBEDDED_C_PLATFORM_VALIDATION_INTERNAL_H
