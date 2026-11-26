/**
 * @file bsp_dwt.c
 * @brief DWT 周期计数与毫秒/微秒延时实现。
 * @details 基于 Cortex-M CoreDebug 与 DWT->CYCCNT 实现高精度周期计时与分片忙等，
 *          在 FreeRTOS 调度器运行时将毫秒延时让渡给 osDelay()，无动态分配。
 * @ingroup BSP
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
  // 必须先开启 CoreDebug 跟踪使能位（TRCENA），否则 DWT 外设寄存器写操作无效
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  // 清零周期计数器后使能 CYCCNT，仅在系统启动阶段调用一次
  DWT->CYCCNT = 0U;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t BSP_DWT_GetCycles(void) { return DWT->CYCCNT; }

uint32_t BSP_DWT_CyclesToUs(uint32_t cycles) {
  if (SystemCoreClock == 0U) {
    return 0U;
  }
  // 使用 64 位中间乘法，避免 cycles * 1000000 在 168 MHz 下超过 4.29e9 而发生 32 位溢出
  return (uint32_t)(((uint64_t)cycles * 1000000ULL) / (uint64_t)SystemCoreClock);
}

void BSP_DWT_DelayUs(uint32_t us) {
  uint32_t cycles_per_us = SystemCoreClock / 1000000U;
  if (us == 0U || cycles_per_us == 0U) {
    return;
  }

#if defined(BSP_HOST_STUB)
  // 主机单元测试桩无真实硬件时钟推进，直接累加 CYCCNT 模拟时间流逝
  DWT->CYCCNT += (uint32_t)((uint64_t)us * (uint64_t)cycles_per_us);
#else
  // 按 1000 us 分块等待，避免大 us 值乘主频后溢出 32 位回绕周期
  uint32_t remaining_us = us;
  while (remaining_us > 0U) {
    uint32_t chunk_us = (remaining_us > 1000U) ? 1000U : remaining_us;
    uint32_t wait_cycles = chunk_us * cycles_per_us;
    uint32_t start = DWT->CYCCNT;
    // 无符号减法天然免疫 32 位计数器回绕；加信号栅栏防止编译器将空循环优化掉
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
  // 仅当 RTOS 调度器已运行时让出 CPU，启动前（如 Board_BootServices 阶段）仍走忙等
  if (osKernelGetState() == osKernelRunning) {
    (void)osDelay(ms);
    return;
  }
#endif

  // 逐毫秒调用 1000 us 忙等，兼顾裸机精度与防溢出
  for (uint32_t i = 0U; i < ms; ++i) {
    BSP_DWT_DelayUs(1000U);
  }
}
