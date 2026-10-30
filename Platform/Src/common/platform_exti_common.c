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
 * @brief 把单 bit 引脚掩码转换为 EXTI 线路下标。
 * @param pin 引脚掩码；必须非零且恰好只有一个 bit 置位。
 * @param index 线路下标输出地址；允许为 NULL，此时仅做校验。
 * @return 掩码合法且线路位于 0..15 时返回 true，否则返回 false。
 * @post 成功且 index 非空时写入 0..15 的下标；失败时不写 index。
 * @note 纯映射函数，不访问硬件或共享状态，可从任务或 ISR 上下文调用。
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
 * @param GPIO_Pin 非零单 bit 引脚掩码，对应 EXTI 线 0..15。
 * @param cb 回调条目；函数复制结构和函数指针，不接管 context 的所有权。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该 EXTI 线已注册。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT GPIO_Pin 为非零单 bit 之外的掩码，cb 为空，
 *         或映射结果超出 16 条 EXTI 线。
 * @pre 应在对应 EXTI 线和 NVIC 使能前完成注册。
 * @note 回调在 EXTI 中断上下文执行，参数是触发中断的 GPIO_Pin；不得阻塞。
 * @note 一个 EXTI 线只允许一个回调所有者，不支持静默覆盖。
 * @note cb 只需在调用期间有效，context 生命周期由调用方保证；不分配内存。
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
 * @param GPIO_Pin 触发中断的单 bit 引脚掩码。
 * @post 对应线路已注册且回调非空时调用一次回调；非法、未注册或空回调时静默返回。
 * @note 仅供 HAL EXTI 回调在 ISR 上下文调用；不得阻塞或执行长耗时解析。
 * @note 注册状态在运行期不修改，本函数不分配内存。
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
