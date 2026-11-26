/**
 * @file sys_time_window.h
 * @brief 硬件无关的 32 位回绕安全毫秒时间窗口比较（窗口需小于 2^31 ms）。
 * @addtogroup sys_core
 * @{
 */

#ifndef LAYERED_CAR_SYS_TIME_WINDOW_H
#define LAYERED_CAR_SYS_TIME_WINDOW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief 判断 now_ms 是否已达到或越过 deadline_ms（支持 32 位回绕）。
 */
static inline bool Sys_Time_HasReachedMs(uint32_t now_ms, uint32_t deadline_ms) {
  return (uint32_t)(now_ms - deadline_ms) < 0x80000000U;
}

/**
 * @brief 判断自 start_ms 起是否已流逝至少 duration_ms 毫秒。
 */
static inline bool Sys_Time_HasElapsedMs(uint32_t now_ms, uint32_t start_ms, uint32_t duration_ms) {
  return (uint32_t)(now_ms - start_ms) >= duration_ms;
}

/**
 * @brief 判断 timestamp_ms 是否严格晚于 reference_ms（半周期窗口内）。
 */
static inline bool Sys_Time_IsAfterMs(uint32_t timestamp_ms, uint32_t reference_ms) {
  uint32_t delta_ms = timestamp_ms - reference_ms;
  return delta_ms > 0U && delta_ms < 0x80000000U;
}

/**
 * @brief 计算 end_ms - start_ms 的 32 位无符号毫秒差值。
 */
static inline uint32_t Sys_Time_ElapsedMs(uint32_t start_ms, uint32_t end_ms) {
  return end_ms - start_ms;
}

/**
 * @brief 判断 timestamp_ms 距今是否已严格超过 timeout_ms（age > timeout_ms）。
 */
static inline bool Sys_Time_IsExpiredMs(uint32_t now_ms, uint32_t timestamp_ms,
                                        uint32_t timeout_ms) {
  return (uint32_t)(now_ms - timestamp_ms) > timeout_ms;
}

/**
 * @brief 非阻塞周期门控：首次（*last_ms == 0）或满 period_ms 时更新 *last_ms 并返回 true。
 */
static inline bool Sys_Time_TryStepPeriodicMs(uint32_t *last_ms, uint32_t now_ms,
                                              uint32_t period_ms) {
  if (last_ms == NULL) {
    return false;
  }
  if (*last_ms != 0U && !Sys_Time_HasElapsedMs(now_ms, *last_ms, period_ms)) {
    return false;
  }
  *last_ms = now_ms;
  return true;
}

/** @} */

#endif /* LAYERED_CAR_SYS_TIME_WINDOW_H */
