//
// Created by Buns_ on 2026/8/16.
//

#include "../Inc/system_delay.h"
#include "stm32f4xx_hal.h"

void System_Delay_Init(void) {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;  //!< 啥意思，为啥要归零
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void System_Delay_Ms(uint32_t ms) {
  #ifdef USE_FREERTOS
    vTaskDelay(pdMS_TO_TICKS(ms));
  #else
    HAL_Delay(ms);
  #endif
}

void System_Delay_Us(uint32_t us) {
  uint32_t startTick = DWT->CYCCNT;
  uint32_t delayTicks = us * (SystemCoreClock / 1000000);
  while (DWT->CYCCNT - startTick < delayTicks) {
    __asm volatile("" ::: "memory"); //!< 防止编译器优化导致执行顺序错误
  }
}
