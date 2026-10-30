/**
 * @file system_critical_contract_test.c
 * @brief System 临界区调度器前后路径契约测试。
 * @ingroup tests
 */

#include "system_critical.h"
#include "FreeRTOS.h"
#include <assert.h>
#include <stdint.h>

uint32_t g_test_primask;
uint32_t g_test_scheduler_state;
uint32_t g_test_task_enter_count;
uint32_t g_test_task_exit_count;
uint32_t g_test_isr_enter_count;
uint32_t g_test_isr_exit_count;

/**
 * @brief 返回测试注入的 FreeRTOS 调度器状态。
 * @return g_test_scheduler_state 的值；0 表示未启动，1 表示运行中。
 * @post 不修改调度器、中断掩码或测试计数。
 * @note 这是 FreeRTOS API 桩，不查询真实调度器，也不会失败。
 */
uint32_t xTaskGetSchedulerState(void) {
  return g_test_scheduler_state;
}

/**
 * @brief 返回测试注入的 PRIMASK 快照。
 * @return g_test_primask 的当前值；0 表示中断未屏蔽，1 表示已屏蔽。
 * @post 不修改 PRIMASK 测试状态或硬件寄存器。
 * @note 该桩直接返回普通内存变量，不读取 Cortex-M 的 PRIMASK。
 */
uint32_t __get_PRIMASK(void) {
  return g_test_primask;
}

/**
 * @brief 在测试中设置 PRIMASK，模拟关闭中断。
 * @post g_test_primask 被置为 1，供 Sys_Critical_Enter 保存和恢复。
 * @note 该桩不执行真实的 CPSID 指令，也不影响主机进程中断。
 */
void __disable_irq(void) {
  g_test_primask = 1u;
}

/**
 * @brief 在测试中写入指定 PRIMASK 值。
 * @param primask 要保存到测试状态中的中断掩码值；不做符号或位宽校验。
 * @post g_test_primask 等于 primask。
 * @note 该桩只模拟中断状态恢复，不操作 Cortex-M 寄存器。
 */
void __set_PRIMASK(uint32_t primask) {
  g_test_primask = primask;
}

/**
 * @brief 重置临界区测试状态。
 * @param primask 初始 PRIMASK 测试值；0 表示中断未屏蔽，1 表示已屏蔽。
 * @param scheduler_state 调度器桩状态；测试使用 taskSCHEDULER_NOT_STARTED 或 taskSCHEDULER_RUNNING。
 * @post PRIMASK 与调度器状态被写入，任务和 ISR 进入/退出计数全部清零。
 * @note 本函数不使用断言；非法调度器值会被原样保存并由后续被测路径读取。
 */
static void reset_test_state(uint32_t primask, uint32_t scheduler_state) {
  g_test_primask = primask;
  g_test_scheduler_state = scheduler_state;
  g_test_task_enter_count = 0u;
  g_test_task_exit_count = 0u;
  g_test_isr_enter_count = 0u;
  g_test_isr_exit_count = 0u;
}

/**
 * @brief 验证启动阶段嵌套临界区的恢复。
 * @post 两层进入后 PRIMASK 保持 1，内层退出仍为 1，外层退出恢复初始值 0；任务临界区计数保持 0。
 * @note 断言失败会立即终止测试进程，因此函数不会返回部分通过状态。
 * @warning 该用例只验证 PRIMASK 分支，不覆盖调度器运行后的 FreeRTOS 临界区。
 */
static void test_boot_nested_restore(void) {
  reset_test_state(0u, taskSCHEDULER_NOT_STARTED);

  Sys_Critical_Enter();
  assert(g_test_primask == 1u);
  Sys_Critical_Enter();
  assert(g_test_primask == 1u);
  Sys_Critical_Exit();
  assert(g_test_primask == 1u);
  Sys_Critical_Exit();
  assert(g_test_primask == 0u);
  assert(g_test_task_enter_count == 0u);
  assert(g_test_task_exit_count == 0u);
}

/**
 * @brief 验证启动阶段保留已有中断掩码。
 * @post 进入与退出后 PRIMASK 均保持调用前的 1，未错误地重新开中断。
 * @note 断言失败会立即终止测试进程。
 * @warning 该用例不覆盖嵌套计数或调度器运行路径。
 */
static void test_boot_preserves_existing_mask(void) {
  reset_test_state(1u, taskSCHEDULER_NOT_STARTED);

  Sys_Critical_Enter();
  assert(g_test_primask == 1u);
  Sys_Critical_Exit();
  assert(g_test_primask == 1u);
}

/**
 * @brief 验证调度器运行时的任务临界区路径。
 * @post 进入和退出任务临界区各使对应桩计数加 1，PRIMASK 保持 0。
 * @note 断言失败会立即终止测试进程。
 * @note FreeRTOS 宏由测试桩替换，只记录调用次数，不进行真实调度或中断控制。
 */
static void test_running_scheduler_uses_task_critical(void) {
  reset_test_state(0u, taskSCHEDULER_RUNNING);

  Sys_Critical_Enter();
  assert(g_test_task_enter_count == 1u);
  assert(g_test_primask == 0u);
  Sys_Critical_Exit();
  assert(g_test_task_exit_count == 1u);
  assert(g_test_primask == 0u);
}

/**
 * @brief 验证 ISR 路径的临界区行为。
 * @post ISR 进入和退出桩计数各为 1，保存状态按 EnterFromISR 返回值传入退出函数。
 * @note 断言失败会立即终止测试进程。
 * @note 测试桩不修改 PRIMASK，也不阻塞或触发真实中断。
 */
static void test_isr_path(void) {
  reset_test_state(0u, taskSCHEDULER_RUNNING);

  sys_critical_state_t state = Sys_Critical_EnterFromISR();
  Sys_Critical_ExitFromISR(state);
  assert(g_test_isr_enter_count == 1u);
  assert(g_test_isr_exit_count == 1u);
}

/**
 * @brief 依次运行启动、任务和 ISR 临界区契约用例。
 * @return 所有用例完成时返回 0。
 * @post 最后一个用例留下 ISR 进入/退出计数各 1、任务进入/退出计数各 0 的观测状态。
 * @note 任一断言失败都会立即终止测试进程，不会返回失败状态。
 * @warning 本测试通过 FreeRTOS 桩验证控制流，不覆盖真实硬件中断屏蔽时序。
 */
int main(void) {
  test_boot_nested_restore();
  test_boot_preserves_existing_mask();
  test_running_scheduler_uses_task_critical();
  test_isr_path();
  return 0;
}
