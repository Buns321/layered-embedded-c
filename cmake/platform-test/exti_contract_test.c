/**
 * @file exti_contract_test.c
 * @brief EXTI Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_exti.h"
#include "platform_exti_backend.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static uint32_t s_callback_count;

/**
 * @brief 记录 EXTI 中断事件。
 */
static void exti_callback(uint16_t pin, void *context) {
  assert(pin == 0x0004U);
  assert(context == &s_callback_count);
  s_callback_count++;
}

/**
 * @brief 验证 EXTI 所有权和事件分发。
 */
static void test_exti_contract(void) {
  s_callback_count = 0U;
  const platform_exti_callback_binding_t callbacks = {
    .exti_cb = exti_callback,
    .context = &s_callback_count,
  };

  assert(Platform_EXTI_RegisterCallback(0x0004U, &callbacks) == PLATFORM_STATUS_OK);
  assert(Platform_EXTI_RegisterCallback(0x0004U, &callbacks) ==
         PLATFORM_STATUS_BUSY);
  assert(Platform_EXTI_RegisterCallback(0U, &callbacks) ==
         PLATFORM_STATUS_INVALID_ARGUMENT);
  assert(Platform_EXTI_RegisterCallback(0x0004U, NULL) ==
         PLATFORM_STATUS_INVALID_ARGUMENT);

  platform_exti_dispatch_event(0x0004U);
  assert(s_callback_count == 1U);
  platform_exti_dispatch_event(0x0008U);
  assert(s_callback_count == 1U);
}

/**
 * @brief 运行本文件定义的契约测试并返回进程状态。
 */
int main(void) {
  test_exti_contract();
  return 0;
}
