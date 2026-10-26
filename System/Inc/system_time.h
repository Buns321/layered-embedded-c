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

/** @brief 读取 DWT 周期计数。 */
static inline uint32_t Sys_Time_GetCycles(void) {
  return DWT->CYCCNT;
}

/**
 * @brief 把 32 位周期差换算为微秒。
 * @param cycles 两个无符号周期计数的差值。
 */
static inline uint32_t Sys_Time_CyclesToUs(uint32_t cycles) {
  return (uint32_t)(
    ((uint64_t)cycles * 1000000ULL) / (uint64_t)SystemCoreClock
  );
}

/** @brief 读取相对 DWT 启动点的 32 位微秒时间。 */
static inline uint32_t Sys_Time_GetUs(void) {
  return Sys_Time_CyclesToUs(Sys_Time_GetCycles());
}

/**
 * @brief 计算两个微秒时间戳的无符号差。
 * @param start_us 起始时间。
 * @param end_us 结束时间。
 */
static inline uint32_t Sys_Time_ElapsedUs(
  uint32_t start_us,
  uint32_t end_us
) {
  return end_us - start_us;
}

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_TIME_H
