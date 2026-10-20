//
// Created by Buns_ on 2026/8/16.
//

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

SYS_DELAY_INLINE void Sys_Delay_Init(void) {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

SYS_DELAY_INLINE void Sys_Delay_Us(uint32_t us) {
  uint32_t startTick = DWT->CYCCNT;
  uint32_t delayTicks = us * (SystemCoreClock / 1000000);
  while (DWT->CYCCNT - startTick < delayTicks) {
    __asm volatile("" ::: "memory"); //!< 防止编译器优化导致执行顺序错误
  }
}

SYS_DELAY_INLINE void Sys_Delay_Ms(uint32_t ms) {
#ifdef USE_FREERTOS
  osDelay(ms);
#else
  for (uint32_t i = 0; i < ms; i++) {
    Sys_Delay_Us(1000u);
  }
#endif
}

#endif //LAYERED_EMBEDDED_C_SYSTEM_DELAY_H
