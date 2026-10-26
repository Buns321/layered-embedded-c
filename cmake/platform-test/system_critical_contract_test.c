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

uint32_t xTaskGetSchedulerState(void) {
  return g_test_scheduler_state;
}

uint32_t __get_PRIMASK(void) {
  return g_test_primask;
}

void __disable_irq(void) {
  g_test_primask = 1u;
}

void __set_PRIMASK(uint32_t primask) {
  g_test_primask = primask;
}

static void reset_test_state(uint32_t primask, uint32_t scheduler_state) {
  g_test_primask = primask;
  g_test_scheduler_state = scheduler_state;
  g_test_task_enter_count = 0u;
  g_test_task_exit_count = 0u;
  g_test_isr_enter_count = 0u;
  g_test_isr_exit_count = 0u;
}

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

static void test_boot_preserves_existing_mask(void) {
  reset_test_state(1u, taskSCHEDULER_NOT_STARTED);

  Sys_Critical_Enter();
  assert(g_test_primask == 1u);
  Sys_Critical_Exit();
  assert(g_test_primask == 1u);
}

static void test_running_scheduler_uses_task_critical(void) {
  reset_test_state(0u, taskSCHEDULER_RUNNING);

  Sys_Critical_Enter();
  assert(g_test_task_enter_count == 1u);
  assert(g_test_primask == 0u);
  Sys_Critical_Exit();
  assert(g_test_task_exit_count == 1u);
  assert(g_test_primask == 0u);
}

static void test_isr_path(void) {
  reset_test_state(0u, taskSCHEDULER_RUNNING);

  sys_critical_state_t state = Sys_Critical_EnterFromISR();
  Sys_Critical_ExitFromISR(state);
  assert(g_test_isr_enter_count == 1u);
  assert(g_test_isr_exit_count == 1u);
}

int main(void) {
  test_boot_nested_restore();
  test_boot_preserves_existing_mask();
  test_running_scheduler_uses_task_critical();
  test_isr_path();
  return 0;
}
