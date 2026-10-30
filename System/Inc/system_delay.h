//
// 由 Buns_ 创建于 2026/8/16。
//

/**
 * @file system_delay.h
 * @brief 基于 DWT 的微秒延时与 RTOS 毫秒延时。
 * @addtogroup system
 * @{
 * @note 通过 STM32F4xx / STM32F1xx 选择 CMSIS 设备头。
 * @note USE_FREERTOS 定义时，Sys_DelayMs 使用 osDelay。
 * @warning Sys_DelayUs 是忙等，长延时不要放在 ISR 或高优先级任务里。
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_DELAY_H
#define LAYERED_EMBEDDED_C_SYSTEM_DELAY_H

#include <stdint.h>

/*
 * 只取 CMSIS 设备定义（DWT / CoreDebug / SystemCoreClock）。
 * 临时屏蔽 USE_HAL_DRIVER，避免 stm32f4xx.h 把 HAL 全量头带进来。
 */
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

#ifdef USE_FREERTOS
#include "cmsis_os2.h"
#endif

#if defined(__GNUC__) || defined(__clang__)
#  define SYS_DELAY_INLINE static inline __attribute__((always_inline))
#else
#  define SYS_DELAY_INLINE static inline
#endif

/**
 * @brief 使能 DWT 周期计数器，供微秒延时使用。
 * @note 应在第一次调用 Sys_DelayUs 或 System 时间辅助函数前执行。
 * @warning 重复调用会把 CYCCNT 清零，使清零前取得的时间戳与清零后不可比较；
 *          运行期不要重新初始化。
 * @post DWT_CTRL.CYCCNTENA 已置位，且 DWT->CYCCNT 从 0 重新开始计数。
 */
SYS_DELAY_INLINE void Sys_Delay_Init(void) {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief 以 DWT 周期计数器忙等指定微秒数。
 * @param us 目标忙等时长，单位 us；0 表示不等待。
 * @pre Sys_Delay_Init 已执行且 SystemCoreClock 反映当前 CPU 频率。
 * @note 阻塞忙等，只用于短延时；实际时长按 SystemCoreClock / 1 MHz 换算周期。
 * @warning us 过大时周期增量会在 32 位内回绕，等待时长不再可靠；应改用
 *          Sys_DelayMs 或 RTOS 延时。
 * @post 函数返回前已消耗约 us 微秒的 CPU 时间，不主动让出 CPU。
 */
SYS_DELAY_INLINE void Sys_DelayUs(uint32_t us) {
  uint32_t startTick = DWT->CYCCNT;
  uint32_t delayTicks = us * (SystemCoreClock / 1000000);
  while (DWT->CYCCNT - startTick < delayTicks) {
    __asm volatile("" ::: "memory"); //!< 防止编译器优化导致执行顺序错误
  }
}

/**
 * @brief 延时指定毫秒数。
 * @param ms 目标延时时长，单位 ms；0 表示不等待。
 * @note USE_FREERTOS 构建使用 osDelay 让出 CPU；裸机构建按 1 ms 分片调用
 *       Sys_DelayUs 忙等。
 * @warning FreeRTOS 分支要求内核已启动；裸机分支长时间忙等会阻塞所有任务逻辑。
 * @post 函数返回前已等待至少 ms 毫秒；FreeRTOS 分支期间允许其他任务运行。
 */
SYS_DELAY_INLINE void Sys_DelayMs(uint32_t ms) {
#ifdef USE_FREERTOS
  osDelay(ms);
#else
  for (uint32_t i = 0; i < ms; i++) {
    Sys_DelayUs(1000u);
  }
#endif
}

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_DELAY_H
