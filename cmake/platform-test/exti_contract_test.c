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
 * @brief 记录 EXTI 分发器送达的测试事件。
 * @param pin 触发中断的单引脚掩码；断言必须为 0x0004U。
 * @param context 注册时传入的上下文；断言必须为 &s_callback_count。
 * @pre 必须由 platform_exti_dispatch_event 向已注册的 0x0004U 引脚分发事件。
 * @post s_callback_count 加一，不保存其他事件数据。
 * @warning 任一 assert 失败都会终止测试进程；该测试在任务上下文同步调用回调，
 *          不模拟真实 EXTI 中断优先级或硬件清标志行为。
 * @note 回调不阻塞、不分配内存，也不访问 GPIO/EXTI 寄存器。
 */
static void exti_callback(uint16_t pin, void *context) {
  assert(pin == 0x0004U);
  assert(context == &s_callback_count);
  s_callback_count++;
}

/**
 * @brief 验证 EXTI 回调所有权、参数校验和事件分发。
 * @post 成功返回时，0x0004U 的首次注册成功、重复注册返回 BUSY；零引脚和空回调
 *       集合返回 INVALID_ARGUMENT；分发 0x0004U 后 s_callback_count 为 1，
 *       分发未注册的 0x0008U 后计数保持 1。
 * @warning 任一 assert 失败都会终止测试进程，不返回可恢复的错误状态。
 * @note 测试只验证公共分发器与测试回调，不触发真实 EXTI 中断或 GPIO 边沿。
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
 * @brief 运行 EXTI 契约测试并返回进程状态。
 * @return 固定返回 0；只有所有内层断言通过才会到达该返回语句。
 * @post 测试全局计数 s_callback_count 保留为 1。
 * @warning 任一 assert 失败都会终止进程，因此不会返回非零退出码。
 * @note 仅供 CTest 启动本测试可执行文件时调用。
 */
int main(void) {
  test_exti_contract();
  return 0;
}
