/**
 * @file platform_exti_common.c
 * @brief Family-independent EXTI ownership and callback dispatch.
 */

#include "platform_exti.h"
#include "platform_exti_backend.h"
#include <stdbool.h>
#include <stddef.h>

static platform_exti_callback_binding_t exti_cb[PLATFORM_EXTI_LINE_COUNT];
static bool exti_cb_registered[PLATFORM_EXTI_LINE_COUNT];

static bool platform_exti_pin_to_index(uint16_t pin, uint8_t *index) {
  if (
    pin == 0U ||
    (pin & (uint16_t)(pin - 1U)) != 0U ||
    index == NULL
  ) {
    return false;
  }

  uint8_t idx = 0U;
  while (pin >>= 1U) {
    idx++;
  }
  if (idx >= PLATFORM_EXTI_LINE_COUNT) {
    return false;
  }
  *index = idx;
  return true;
}

platform_status_t Platform_EXTI_RegisterCallback(
  uint16_t GPIO_Pin,
  const platform_exti_callback_binding_t *cb
) {
  uint8_t idx;
  if (cb == NULL || !platform_exti_pin_to_index(GPIO_Pin, &idx)) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (exti_cb_registered[idx]) {
    return PLATFORM_STATUS_BUSY;
  }

  exti_cb[idx] = *cb;
  exti_cb_registered[idx] = true;
  return PLATFORM_STATUS_OK;
}

void platform_exti_dispatch_event(uint16_t GPIO_Pin) {
  uint8_t idx;
  if (
    !platform_exti_pin_to_index(GPIO_Pin, &idx) ||
    !exti_cb_registered[idx]
  ) {
    return;
  }
  if (exti_cb[idx].exti_cb != NULL) {
    exti_cb[idx].exti_cb(GPIO_Pin, exti_cb[idx].context);
  }
}
