/**
 * @file system_angle_contract_test.c
 * @brief System 角度归一化契约测试。
 * @ingroup tests
 */

#include "system_angle.h"
#include <assert.h>
#include <math.h>
#include <stddef.h>

/**
 * @brief 判断两个浮点值是否近似相等。
 * @param left 左值。
 * @param right 右值。
 * @return 绝对差不超过 1e-5 时返回 true。
 * @pre 无。
 * @post 不修改参数。
 * @note 测试辅助函数。
 */
static bool near(float left, float right) {
  return fabsf(left - right) <= 1e-5f;
}

/**
 * @brief 验证角度归一化和非有限输入保护。
 * @post deg/rad 的周期等价、边界和失败不写输出行为符合契约。
 * @note 不访问硬件。
 */
static void test_angle_wrap(void) {
  float value = 7.0f;
  assert(Sys_Angle_WrapDegrees(540.0f, &value));
  assert(near(value, 180.0f));
  assert(Sys_Angle_WrapDegrees(-540.0f, &value));
  assert(near(value, -180.0f));
  assert(Sys_Angle_WrapDegrees(190.0f, &value));
  assert(near(value, -170.0f));

  value = 7.0f;
  assert(Sys_Angle_WrapRadians(3.0f * 3.14159265358979323846f, &value));
  assert(near(value, 3.14159265358979323846f));
  assert(Sys_Angle_WrapRadians(-3.0f * 3.14159265358979323846f, &value));
  assert(near(value, -3.14159265358979323846f));

  value = 7.0f;
  assert(!Sys_Angle_WrapDegrees(NAN, &value));
  assert(value == 7.0f);
  assert(!Sys_Angle_WrapRadians(INFINITY, &value));
  assert(value == 7.0f);
  assert(!Sys_Angle_WrapDegrees(0.0f, NULL));
  assert(!Sys_Angle_WrapRadians(0.0f, NULL));
}

/**
 * @brief 运行角度归一化契约测试。
 * @return 全部断言通过后返回 0。
 * @post 测试不保留外部资源。
 * @note 任一断言失败会终止进程。
 */
int main(void) {
  test_angle_wrap();
  return 0;
}
