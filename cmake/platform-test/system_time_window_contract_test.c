/**
 * @file system_time_window_contract_test.c
 * @brief System 毫秒窗口辅助函数契约测试。
 * @ingroup tests
 */

#include "system_time_window.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 验证截止时间和持续时间判断。
 * @post 相等、跨回绕和未到期边界均符合无符号模差语义。
 * @note 纯函数测试，不访问硬件。
 */
static void test_time_windows(void) {
  assert(Sys_Time_HasReachedMs(100u, 100u));
  assert(Sys_Time_HasReachedMs(101u, 100u));
  assert(!Sys_Time_HasReachedMs(99u, 100u));
  assert(Sys_Time_HasReachedMs(0u, UINT32_MAX));

  assert(Sys_Time_HasElapsedMs(100u, 100u, 0u));
  assert(Sys_Time_HasElapsedMs(110u, 100u, 10u));
  assert(!Sys_Time_HasElapsedMs(109u, 100u, 10u));
  assert(Sys_Time_HasElapsedMs(5u, UINT32_MAX - 4u, 10u));
}

/**
 * @brief 运行毫秒窗口契约测试。
 * @return 全部断言通过后返回 0。
 * @post 测试不保留外部资源。
 * @note 任一断言失败会终止进程。
 */
int main(void) {
  test_time_windows();
  return 0;
}
