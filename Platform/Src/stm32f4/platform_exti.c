//
// Created by Buns_ on 2026/8/27.
//

#include "../../Inc/platform_exti.h"
#include <assert.h>

static platform_exti_callbacks_t exti_cb[TOTAL_EXTI_NUM] = {{0}};

static uint8_t pin_to_index(uint16_t pin) {
  uint8_t idx = 0;
  while (pin >>= 1) idx++;
  return idx;
}

void Platform_EXTI_RegisterCallbacks(uint16_t GPIO_Pin, const platform_exti_callbacks_t *cb) {
  assert(cb);
  uint8_t idx = pin_to_index(GPIO_Pin);
  exti_cb[idx] = *cb;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  uint8_t idx = pin_to_index(GPIO_Pin);
  if (exti_cb[idx].exti_cb) exti_cb[idx].exti_cb(idx, exti_cb[idx].context);
}
