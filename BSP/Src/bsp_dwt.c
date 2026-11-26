/**
 * @file bsp_dwt.c
 * @brief DWT 周期计数与毫秒/微秒延时实现。
 * @ingroup bsp
 */

#include "bsp_dwt.h"
#include "bsp_mcu.h"
#include <stdatomic.h>

#ifdef USE_FREERTOS
#if defined(BSP_HOST_STUB)
#include "rtos_stub.h"
#else
#include "cmsis_os2.h"
#endif
#endif

void BSP_DWT_Init(void) {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t BSP_DWT_GetCycles(void) { return DWT->CYCCNT; }

uint32_t BSP_DWT_CyclesToUs(uint32_t cycles) {
  if (SystemCoreClock == 0U) {
    return 0U;
  }
  return (uint32_t)(((uint64_t)cycles * 1000000ULL) / (uint64_t)SystemCoreClock);
}

void BSP_DWT_DelayUs(uint32_t us) {
  uint32_t cycles_per_us = SystemCoreClock / 1000000U;
  if (us == 0U || cycles_per_us == 0U) {
    return;
  }

#if defined(BSP_HOST_STUB)
  DWT->CYCCNT += (uint32_t)((uint64_t)us * (uint64_t)cycles_per_us);
#else
  /* 按 1000 us 分块等待，避免大 us 值乘主频后溢出 32 位回绕周期。 */
  uint32_t remaining_us = us;
  while (remaining_us > 0U) {
    uint32_t chunk_us = (remaining_us > 1000U) ? 1000U : remaining_us;
    uint32_t wait_cycles = chunk_us * cycles_per_us;
    uint32_t start = DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT - start) < wait_cycles) {
      atomic_signal_fence(memory_order_seq_cst);
    }
    remaining_us -= chunk_us;
  }
#endif
}

void BSP_DelayMs(uint32_t ms) {
  if (ms == 0U) {
    return;
  }

#ifdef USE_FREERTOS
  if (osKernelGetState() == osKernelRunning) {
    (void)osDelay(ms);
    return;
  }
#endif

  for (uint32_t i = 0U; i < ms; ++i) {
    BSP_DWT_DelayUs(1000U);
  }
}
