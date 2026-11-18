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
#include <stddef.h>
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
static inline bool Sys_Time_HasReachedMs(uint32_t now_ms, uint32_t deadline_ms) {
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
static inline bool Sys_Time_HasElapsedMs(uint32_t now_ms, uint32_t start_ms, uint32_t duration_ms) {
  return (uint32_t)(now_ms - start_ms) >= duration_ms;
}

/**
 * @brief 判断时间戳是否严格晚于参考时间。
 * @param timestamp_ms 待比较单调毫秒时间戳，单位 ms。
 * @param reference_ms 参考单调毫秒时间戳，单位 ms。
 * @return timestamp_ms 严格晚于 reference_ms 且处于同一半周期窗口内时返回 true。
 * @pre 两个时间戳使用同一时基，距离小于 2^31 ms。
 * @post 纯函数，不修改参数或共享状态。
 * @note 使用无符号模差避免有符号溢出；相等时返回 false。
 */
static inline bool Sys_Time_IsAfterMs(uint32_t timestamp_ms, uint32_t reference_ms) {
  uint32_t delta_ms = timestamp_ms - reference_ms;
  return delta_ms > 0U && delta_ms < 0x80000000U;
}

/**
 * @brief 计算两个毫秒时间戳之间的无符号差值。
 * @param start_ms 起始单调毫秒时间戳，单位 ms。
 * @param end_ms 结束单调毫秒时间戳，单位 ms。
 * @return end_ms - start_ms 的 32 位无符号差值，单位 ms，支持回绕。
 * @pre 两个时间戳使用同一时基，间隔小于 2^32 ms。
 * @post 纯函数，不修改参数或共享状态。
 */
static inline uint32_t Sys_Time_ElapsedMs(uint32_t start_ms, uint32_t end_ms) {
  return end_ms - start_ms;
}

/**
 * @brief 判断给定时间戳是否已超过最大允许年龄。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @param timestamp_ms 被检查事件时间戳，单位 ms。
 * @param timeout_ms 允许的最大时间差，单位 ms；差值严格大于该值时判为过期。
 * @return (uint32_t)(now_ms - timestamp_ms) > timeout_ms 时返回 true。
 * @pre now_ms 与 timestamp_ms 使用同一时基，间隔小于 2^31 ms。
 * @post 纯函数，不修改参数或共享状态。
 * @note 与 Sys_Time_HasElapsedMs 的区别在于边界为严格大于（age > timeout_ms）。
 */
static inline bool Sys_Time_IsExpiredMs(uint32_t now_ms, uint32_t timestamp_ms,
                                        uint32_t timeout_ms) {
  return (uint32_t)(now_ms - timestamp_ms) > timeout_ms;
}

/**
 * @brief 非阻塞周期节流门控：到达周期时更新基准时间戳并返回 true。
 * @param last_ms 上次触发时间戳指针，不得为 NULL；0 表示尚未完成首次非零触发。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @param period_ms 期望触发周期，单位 ms。
 * @retval true 首次触发（*last_ms == 0）或已满 period_ms，*last_ms 已更新为 now_ms。
 * @retval false last_ms 为 NULL 或尚未到达下一个周期，*last_ms 保持不变。
 * @pre *last_ms 与 now_ms 使用同一时基；调用间隔小于 2^31 ms。
 * @post 返回 true 时 *last_ms == now_ms。
 * @note 不分配内存；若首次触发时 now_ms 恰好为 0，下一次非零调用会再触发一次以建立非零基准。
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

#endif // LAYERED_EMBEDDED_C_SYSTEM_TIME_WINDOW_H
