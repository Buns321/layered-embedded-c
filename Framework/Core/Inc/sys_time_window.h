/**
 * @file sys_time_window.h
 * @brief 硬件无关的 32 位回绕安全毫秒时间窗口比较（窗口需小于 2^31 ms）。
 * @details 属于 Layer 2（Framework/Core），纯内联无状态实现，依赖无符号 32 位模运算
 *          （模 2^32）在约 49.7 天回绕边界处保持单调比较正确性；零 HAL/RTOS 依赖。
 * @addtogroup Framework_Core
 * @{
 */

#ifndef LAYERED_CAR_SYS_TIME_WINDOW_H
#define LAYERED_CAR_SYS_TIME_WINDOW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief 判断 now_ms 是否已达到或越过 deadline_ms（支持 32 位回绕）。
 * @param[in] now_ms      当前单调毫秒时间戳（单位：ms）。
 * @param[in] deadline_ms 目标截止毫秒时间戳（单位：ms）。
 * @return 是否已到达或越过截止时刻。
 * @note 要求 now_ms 与 deadline_ms 的差值绝对值小于 2^31 ms（约 24.8 天）。
 */
static inline bool Sys_Time_HasReachedMs(uint32_t now_ms, uint32_t deadline_ms) {
  return (uint32_t)(now_ms - deadline_ms) < 0x80000000U;
}

/**
 * @brief 判断自 start_ms 起是否已流逝至少 duration_ms 毫秒。
 * @param[in] now_ms      当前单调毫秒时间戳（单位：ms）。
 * @param[in] start_ms    起始毫秒时间戳（单位：ms）。
 * @param[in] duration_ms 期望经过的持续时长（单位：ms）。
 * @return 若已流逝 >= duration_ms 返回 true，否则返回 false。
 */
static inline bool Sys_Time_HasElapsedMs(uint32_t now_ms, uint32_t start_ms, uint32_t duration_ms) {
  return (uint32_t)(now_ms - start_ms) >= duration_ms;
}

/**
 * @brief 判断 timestamp_ms 是否严格晚于 reference_ms（半周期窗口内）。
 * @param[in] timestamp_ms 待判定毫秒时间戳（单位：ms）。
 * @param[in] reference_ms 参考基准毫秒时间戳（单位：ms）。
 * @return 若 timestamp_ms 严格晚于 reference_ms（差值在 1..0x7FFFFFFF）返回 true。
 */
static inline bool Sys_Time_IsAfterMs(uint32_t timestamp_ms, uint32_t reference_ms) {
  uint32_t delta_ms = timestamp_ms - reference_ms;
  return delta_ms > 0U && delta_ms < 0x80000000U;
}

/**
 * @brief 计算 end_ms - start_ms 的 32 位无符号毫秒差值。
 * @param[in] start_ms 起始毫秒时间戳（单位：ms）。
 * @param[in] end_ms   结束毫秒时间戳（单位：ms）。
 * @return 经过的毫秒数（单位：ms，自动处理单次 32 位回绕）。
 */
static inline uint32_t Sys_Time_ElapsedMs(uint32_t start_ms, uint32_t end_ms) {
  return end_ms - start_ms;
}

/**
 * @brief 判断 timestamp_ms 距今是否已严格超过 timeout_ms（age > timeout_ms）。
 * @param[in] now_ms       当前单调毫秒时间戳（单位：ms）。
 * @param[in] timestamp_ms 待检测事件发生时刻（单位：ms）。
 * @param[in] timeout_ms   超时阈值（单位：ms）。
 * @return 若 (now_ms - timestamp_ms) > timeout_ms 返回 true，否则返回 false。
 */
static inline bool Sys_Time_IsExpiredMs(uint32_t now_ms, uint32_t timestamp_ms,
                                        uint32_t timeout_ms) {
  return (uint32_t)(now_ms - timestamp_ms) > timeout_ms;
}

/**
 * @brief 非阻塞周期门控：首次（*last_ms == 0）或满 period_ms 时更新 *last_ms 并返回 true。
 * @param[in,out] last_ms   保存上次触发时刻的指针（允许为 NULL，为 NULL 时返回 false）。
 * @param[in]     now_ms    当前单调毫秒时间戳（单位：ms）。
 * @param[in]     period_ms 目标触发周期（单位：ms）。
 * @return 本次调用是否满足触发条件。
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
