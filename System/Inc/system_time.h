//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file system_time.h
 * @brief 基于 DWT 周期计数器的单调时间辅助。
 * @addtogroup system
 * @{
 * @note 调用前必须执行 Sys_Delay_Init() 启用 DWT CYCCNT。
 * @note 32 位周期计数器在 72 MHz 下约 59.65 s 回绕，差值必须使用无符号减法。
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_TIME_H
#define LAYERED_EMBEDDED_C_SYSTEM_TIME_H

#include "system_time_window.h"
#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
#  pragma push_macro("USE_HAL_DRIVER")
#  undef USE_HAL_DRIVER
#endif
#if defined(STM32F4xx)
#  include "stm32f4xx.h"
#elif defined(STM32F1xx)
#  include "stm32f1xx.h"
#else
#  error "Unsupported STM32 family"
#endif
#if defined(__GNUC__) || defined(__clang__)
#  pragma pop_macro("USE_HAL_DRIVER")
#endif

/**
 * @brief 读取 DWT 周期计数。
 * @return 当前 DWT->CYCCNT 的 32 位计数值，单位 cycle。
 * @pre Sys_Delay_Init 已使能 DWT 周期计数器。
 * @note 只读取硬件计数器，不修改状态；32 位计数在 SystemCoreClock 下周期回绕。
 */
static inline uint32_t Sys_Time_GetCycles(void) {
  return DWT->CYCCNT;
}

/**
 * @brief 把 32 位周期差换算为微秒。
 * @param cycles 两个时间戳按无符号减法得到的周期差，单位 cycle。
 * @return 周期差对应的整数微秒数，单位 us，向零截断。
 * @pre SystemCoreClock 非 0 且反映采样期间的 CPU 频率。
 * @note 使用 64 位中间乘积，覆盖完整 32 位周期差；调用方不要把已换算时间戳
 *       再相减后当作周期差传入。
 */
static inline uint32_t Sys_Time_CyclesToUs(uint32_t cycles) {
  return (uint32_t)(
    ((uint64_t)cycles * 1000000ULL) / (uint64_t)SystemCoreClock
  );
}

/**
 * @brief 读取相对 DWT 启动点的 32 位微秒时间。
 * @return 相对 DWT CYCCNT 零点的微秒时间，单位 us。
 * @pre Sys_Delay_Init 已执行，且 SystemCoreClock 反映当前 CPU 频率。
 * @note 返回值会随 DWT 周期计数器回绕，不能直接比较先后；区间测量应使用
 *       Sys_Time_ElapsedUs。
 */
static inline uint32_t Sys_Time_GetUs(void) {
  return Sys_Time_CyclesToUs(Sys_Time_GetCycles());
}

/**
 * @brief 计算两个微秒时间戳的无符号差。
 * @param start_us 区间起点的 Sys_Time_GetUs 返回值，单位 us。
 * @param end_us 区间终点的 Sys_Time_GetUs 返回值，单位 us。
 * @return end_us - start_us 的 32 位无符号结果，单位 us，可跨越零点回绕。
 * @pre 区间长度短于 DWT 周期计数器的一次完整回绕周期。
 * @warning 区间超过回绕周期时结果会错误地表现为短延时；超长区间应使用
 *          更高位宽或 RTOS 时基。
 */
static inline uint32_t Sys_Time_ElapsedUs(
  uint32_t start_us,
  uint32_t end_us
) {
  return end_us - start_us;
}

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_TIME_H
