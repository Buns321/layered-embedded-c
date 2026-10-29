/**
 * @file platform_exti_common.c
 * @brief 与芯片系列无关的 EXTI 所有权管理和回调分发。
 * @ingroup platform
 */

#include "platform_exti.h"
#include "platform_exti_backend.h"
#include <stdbool.h>
#include <stddef.h>

static platform_exti_callback_binding_t exti_cb[PLATFORM_EXTI_LINE_COUNT];
static bool exti_cb_registered[PLATFORM_EXTI_LINE_COUNT];

/**
 * @brief 把引脚掩码转换为 EXTI 线路下标。
 */
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

/**
 * @brief 注册某个引脚的 EXTI 回调。
 * @param GPIO_Pin 单引脚掩码，必须是非零单 bit。
 * @param cb 回调条目，内容会被复制。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该 EXTI 线已注册。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 引脚或回调条目非法。
 * @note 回调在 EXTI 中断上下文执行，参数是触发中断的 GPIO Pin。
 * @note 一个 EXTI 线只允许一个回调所有者，不支持静默覆盖。
 */
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

/**
 * @brief 把 EXTI 引脚事件分发给已注册回调。
 */
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
