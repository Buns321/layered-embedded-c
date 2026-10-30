/**
 * @file controller_pid_contract_test.c
 * @brief 位置式 PID 控制律的主机原生契约测试。
 * @ingroup tests
 */

#include "controller_pid.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

#define CHECK(condition) do {                                                \
  if (!(condition)) {                                                        \
    (void)fprintf(                                                           \
      stderr,                                                                \
      "%s:%d: check failed: %s\n",                                           \
      __FILE__,                                                              \
      __LINE__,                                                              \
      #condition                                                             \
    );                                                                       \
    return 1;                                                                \
  }                                                                          \
} while (0)

/**
 * @brief 构造 PID 控制器测试配置。
 * @param controller 非空 PID 对象；调用后完成初始化并清空运行状态。
 * @param kp 比例增益，单位 duty/被控量；必须有限且非负。
 * @param ki 积分增益，单位 duty/(被控量·s)；必须有限且非负。
 * @param kd 微分增益，单位 duty·s/被控量；必须有限且非负。
 * @param max_out 输出上限，单位 duty；必须有限且不小于 min_out。
 * @param min_out 输出下限，单位 duty；必须有限且不大于 0。
 * @param max_integral 积分项绝对值上限，单位 duty；必须有限且非负。
 * @param derivative_filter_tau 微分一阶低通时间常数，单位 s；必须有限且非负。
 * @pre controller 非空，全部数值满足 Ctrl_PID_Init 的有限性与范围断言。
 * @post 控制器使用名称 "test"、固定目标范围 -10..10 和调用方提供的其余参数。
 * @warning 参数违反上述不变量时 Ctrl_PID_Init 内部断言失败并终止进程。
 * @note 固定目标范围是为了让各用例仅关注待验证的控制项。
 */
static void init_pid(
  controller_pid_t *controller,
  float kp,
  float ki,
  float kd,
  float max_out,
  float min_out,
  float max_integral,
  float derivative_filter_tau
) {
  Ctrl_PID_Init(
    controller,
    "test",
    kp,
    ki,
    kd,
    10.0f,
    -10.0f,
    max_out,
    min_out,
    max_integral,
    derivative_filter_tau
  );
}

/**
 * @brief 验证 PID 比例项和分派。
 * @retval 0 两次更新均通过检查。
 * @retval 1 任一次状态或输出不符合预期；CHECK 已向 stderr 输出位置和条件。
 * @post 控制器经历两次更新：目标 1、测量 0 时输出约 100 duty；
 *       目标 1、测量 1 时输出约 0 duty，确认经 controller_base 正确分派。
 * @note 使用 CHECK 而非 assert；失败时当前测试函数立即返回 1，不终止整个进程。
 */
static int test_proportional_and_dispatch(void) {
  controller_pid_t controller = {0};
  init_pid(
    &controller,
    100.0f,
    0.0f,
    0.0f,
    1000.0f,
    -1000.0f,
    0.0f,
    0.0f
  );
  float out = 0.0f;

  CHECK(
    Ctrl_Base_Update(&controller.controller_base, 1.0f, 0.0f, 0.01f, &out) ==
    CONTROLLER_STATUS_OK
  );
  CHECK(fabsf(out - 100.0f) < 0.001f);

  CHECK(
    Ctrl_Base_Update(&controller.controller_base, 1.0f, 1.0f, 0.01f, &out) ==
    CONTROLLER_STATUS_OK
  );
  CHECK(fabsf(out) < 0.001f);
  return 0;
}

/**
 * @brief 验证 PID 积分项与限幅。
 * @retval 0 积分输出与积分项限幅检查全部通过。
 * @retval 1 任一输出或积分项不符合预期；CHECK 已向 stderr 输出诊断。
 * @post 首次 0.1 s 更新输出约 10 duty；继续十次后输出和积分项均被限制在约 25 duty。
 * @note 控制周期为 0.1 s，积分增益为 100 duty/(被控量·s)；
 *       失败时返回 1，不终止整个测试进程。
 */
static int test_integral_and_limit(void) {
  controller_pid_t controller = {0};
  init_pid(
    &controller,
    0.0f,
    100.0f,
    0.0f,
    1000.0f,
    -1000.0f,
    25.0f,
    0.0f
  );
  float out = 0.0f;

  CHECK(
    Ctrl_Base_Update(&controller.controller_base, 1.0f, 0.0f, 0.1f, &out) ==
    CONTROLLER_STATUS_OK
  );
  CHECK(fabsf(out - 10.0f) < 0.001f);

  for (uint8_t i = 0u; i < 10u; i++) {
    (void)Ctrl_Base_Update(
      &controller.controller_base,
      1.0f,
      0.0f,
      0.1f,
      &out
    );
  }

  CHECK(fabsf(out - 25.0f) < 0.001f);
  CHECK(fabsf(controller.integral - 25.0f) < 0.001f);
  return 0;
}

/**
 * @brief 验证 PID 输出饱和与抗积分饱和。
 * @retval 0 饱和状态、输出上限、积分上限和反向恢复检查全部通过。
 * @retval 1 任一状态或数值不符合预期；CHECK 已向 stderr 输出诊断。
 * @post 从第六帧起，连续正误差使输出保持 50 duty 上限且积分不超过约 50 duty；
 *       随后负误差使状态恢复 OK，输出低于 50 duty，验证条件积分可退出饱和。
 * @note 输出每 0.1 s 更新一次；失败时返回 1，不终止整个测试进程。
 */
static int test_output_saturation_antwindup(void) {
  controller_pid_t controller = {0};
  init_pid(
    &controller,
    0.0f,
    100.0f,
    0.0f,
    50.0f,
    -50.0f,
    1000.0f,
    0.0f
  );
  float out = 0.0f;

  for (uint8_t i = 0u; i < 20u; i++) {
    controller_status_t status = Ctrl_Base_Update(
      &controller.controller_base,
      1.0f,
      0.0f,
      0.1f,
      &out
    );

    if (i >= 5u) {
      CHECK(status == CONTROLLER_STATUS_SATURATED);
    }
  }

  CHECK(fabsf(out - 50.0f) < 0.001f);
  CHECK(controller.integral <= 50.001f);

  controller_status_t status = Ctrl_Base_Update(
    &controller.controller_base,
    0.0f,
    1.0f,
    0.1f,
    &out
  );
  CHECK(status == CONTROLLER_STATUS_OK);
  CHECK(out < 50.0f);
  return 0;
}

/**
 * @brief 验证测量值微分与首帧处理。
 * @retval 0 首帧零微分和第二帧微分方向检查通过。
 * @retval 1 任一状态或输出不符合预期；CHECK 已向 stderr 输出诊断。
 * @post 首个测量样本因无历史值而输出 0；测量从 0 增至 0.1、dt=0.1 s 时，
 *       微分作用于测量值并输出约 -100 duty。
 * @note 本用例只覆盖测量变化路径，未单独改变目标；失败时返回 1，不终止整个进程。
 */
static int test_derivative_on_measurement(void) {
  controller_pid_t controller = {0};
  init_pid(
    &controller,
    0.0f,
    0.0f,
    100.0f,
    1000.0f,
    -1000.0f,
    0.0f,
    0.0f
  );
  float out = 0.0f;

  CHECK(
    Ctrl_Base_Update(&controller.controller_base, 0.0f, 0.0f, 0.1f, &out) ==
    CONTROLLER_STATUS_OK
  );
  CHECK(fabsf(out) < 0.001f);

  CHECK(
    Ctrl_Base_Update(&controller.controller_base, 0.0f, 0.1f, 0.1f, &out) ==
    CONTROLLER_STATUS_OK
  );
  CHECK(fabsf(out + 100.0f) < 0.01f);
  return 0;
}

/**
 * @brief 验证微分低通滤波。
 * @retval 0 滤波后的微分输出符合一阶低通预期。
 * @retval 1 状态或输出不符合预期；CHECK 已向 stderr 输出诊断。
 * @post 在 dt=0.1 s、tau=0.1 s 时 alpha 为 0.5；
 *       第二帧测量变化产生的 -100 duty 微分被滤为约 -50 duty。
 * @note 首个更新只建立测量历史；失败时返回 1，不终止整个进程。
 */
static int test_derivative_filter(void) {
  controller_pid_t controller = {0};
  init_pid(
    &controller,
    0.0f,
    0.0f,
    100.0f,
    1000.0f,
    -1000.0f,
    0.0f,
    0.1f
  );
  float out = 0.0f;

  (void)Ctrl_Base_Update(
    &controller.controller_base,
    0.0f,
    0.0f,
    0.1f,
    &out
  );
  CHECK(
    Ctrl_Base_Update(&controller.controller_base, 0.0f, 0.1f, 0.1f, &out) ==
    CONTROLLER_STATUS_OK
  );
  CHECK(fabsf(out + 50.0f) < 0.01f);
  return 0;
}

/**
 * @brief 验证 PID 目标限幅和非法输入。
 * @retval 0 目标限幅、饱和状态和 NaN 错误清理检查全部通过。
 * @retval 1 任一状态、输出或内部状态不符合预期；CHECK 已向 stderr 输出诊断。
 * @post 目标 20 被限制到上限 10，输出约 100 duty 并返回 SATURATED；
 *       NaN 目标返回 ERROR、输出清零，并清空积分与测量历史标志。
 * @note 该用例确认非法输入会丢弃旧控制状态；失败时返回 1，不终止整个进程。
 */
static int test_target_clamp_and_invalid_input(void) {
  controller_pid_t controller = {0};
  init_pid(
    &controller,
    10.0f,
    0.0f,
    0.0f,
    1000.0f,
    -1000.0f,
    0.0f,
    0.0f
  );
  float out = 0.0f;

  CHECK(
    Ctrl_Base_Update(&controller.controller_base, 20.0f, 0.0f, 0.01f, &out) ==
    CONTROLLER_STATUS_SATURATED
  );
  CHECK(fabsf(out - 100.0f) < 0.001f);

  CHECK(
    Ctrl_Base_Update(&controller.controller_base, NAN, 0.0f, 0.01f, &out) ==
    CONTROLLER_STATUS_ERROR
  );
  CHECK(fabsf(out) < 0.001f);
  CHECK(!controller.has_last_measure);
  CHECK(controller.integral == 0.0f);
  return 0;
}

/**
 * @brief 验证 PID 重置。
 * @retval 0 重置后的内部状态全部符合预期。
 * @retval 1 重置前状态或重置后字段不符合预期；CHECK 已向 stderr 输出诊断。
 * @post 一次积分更新建立非零积分和有效测量历史；Ctrl_Base_Reset 后积分、
 *       滤波微分、上次输出和测量历史均清零。
 * @note 失败时返回 1，不终止整个测试进程。
 */
static int test_reset(void) {
  controller_pid_t controller = {0};
  init_pid(
    &controller,
    0.0f,
    100.0f,
    0.0f,
    1000.0f,
    -1000.0f,
    100.0f,
    0.0f
  );
  float out = 0.0f;

  (void)Ctrl_Base_Update(
    &controller.controller_base,
    1.0f,
    0.0f,
    0.1f,
    &out
  );
  CHECK(controller.integral > 0.0f);
  CHECK(controller.has_last_measure);

  Ctrl_Base_Reset(&controller.controller_base);
  CHECK(controller.integral == 0.0f);
  CHECK(controller.filtered_derivative == 0.0f);
  CHECK(controller.last_out == 0.0f);
  CHECK(!controller.has_last_measure);
  return 0;
}

/**
 * @brief 运行本文件定义的契约测试并返回进程状态。
 * @retval 0 七个 PID 契约测试均通过，并向 stdout 输出通过消息。
 * @retval 1 任一子测试返回非零；主函数立即停止后续测试并返回 1。
 * @post 成功时进程以 0 退出；失败时 CHECK 已向 stderr 写出文件和条件。
 * @note 本测试不使用 assert，也不访问硬件或分配动态内存。
 */
int main(void) {
  if (test_proportional_and_dispatch() != 0) {
    return 1;
  }
  if (test_integral_and_limit() != 0) {
    return 1;
  }
  if (test_output_saturation_antwindup() != 0) {
    return 1;
  }
  if (test_derivative_on_measurement() != 0) {
    return 1;
  }
  if (test_derivative_filter() != 0) {
    return 1;
  }
  if (test_target_clamp_and_invalid_input() != 0) {
    return 1;
  }
  if (test_reset() != 0) {
    return 1;
  }

  (void)puts("controller pid contract tests passed");
  return 0;
}
