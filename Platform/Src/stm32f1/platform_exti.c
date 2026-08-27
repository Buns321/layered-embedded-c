//
// Created by Buns_ on 2026/8/27.
//

#include "../../Inc/platform_exti.h"
#include <assert.h>

static platform_exti_callbacks_t exti_cb[16] = {{0}};

void Platform_EXTI_RegisterCallbacks(uint8_t GPIO_Pin, const platform_exti_callbacks_t *cb) {
  assert(cb);
  exti_cb[GPIO_Pin] = *cb;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  if (exti_cb->exit_cb) exti_cb->exit_cb(GPIO_Pin, exti_cb->context);
}
