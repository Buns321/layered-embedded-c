/**
 * @file system_time_window.h
 * @brief 与硬件时基无关的 32 位毫秒窗口比较。
 * @addtogroup system
 * @{
 * @note 时间戳必须来自同一单调毫秒时基；比较窗口必须小于 2^31 ms。
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_TIME_WINDOW_H
#define LAYERED_EMBEDDED_C_SYSTEM_TIME_WINDOW_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 判断当前毫秒时间是否已经到达截止时间。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @param deadline_ms 目标单调毫秒时间戳，单位 ms。
 * @return now_ms 等于或晚于 deadline_ms，且两者处于同一半周期窗口内时返回 true。
 * @pre now_ms 与 deadline_ms 使用同一时基，距离小于 2^31 ms。
 * @post 纯函数，不修改参数或共享状态。
 * @note 使用无符号模差实现，支持 32 位时基回绕；不分配内存，可在任务或 ISR 调用。
 */
static inline bool Sys_Time_HasReachedMs(
  uint32_t now_ms,
  uint32_t deadline_ms
) {
  return (uint32_t)(now_ms - deadline_ms) < 0x80000000U;
}

/**
 * @brief 判断从起点开始的持续时间是否已经达到。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @param start_ms 起始单调毫秒时间戳，单位 ms。
 * @param duration_ms 目标持续时间，单位 ms；0 表示立即达到。
 * @return 无符号差值 now_ms - start_ms 大于或等于 duration_ms 时返回 true。
 * @pre now_ms 与 start_ms 使用同一时基，距离小于 2^31 ms。
 * @post 纯函数，不修改参数或共享状态。
 * @note 支持 32 位时基回绕；不分配内存，可在任务或 ISR 调用。
 */
static inline bool Sys_Time_HasElapsedMs(
  uint32_t now_ms,
  uint32_t start_ms,
  uint32_t duration_ms
) {
  return (uint32_t)(now_ms - start_ms) >= duration_ms;
}

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_TIME_WINDOW_H
