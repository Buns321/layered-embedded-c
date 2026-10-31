/**
 * @file filter_ekf_contract_test.c
 * @brief 七状态 EKF 壳层与在线参数契约测试。
 * @ingroup tests
 */

#include "filter_base.h"
#include "filter_ekf.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief EKF 契约测试使用的空临界区进入桩。
 * @pre 必须与 Sys_Critical_Exit 配对调用；本测试不在 ISR 中调用。
 * @post 函数体为空，不保存 PRIMASK、不关闭中断、不增加嵌套计数。
 * @warning 该桩不提供互斥或中断保护，只用于让 EKF 公共层完成链接。
 * @note 本测试后续不依赖真实临界区；生产 System 实现才有中断/调度保护语义。
 */
void Sys_Critical_Enter(void) {
}

/**
 * @brief EKF 契约测试使用的空临界区退出桩。
 * @pre 之前已调用本文件的 Sys_Critical_Enter 桩；本函数不验证配对关系。
 * @post 函数体为空，不恢复 PRIMASK、不开启中断、不修改嵌套计数。
 * @warning 该桩不提供互斥或中断保护，只用于让 EKF 公共层完成链接。
 * @note 必须与 Sys_Critical_Enter 成对调用；生产 System 实现才有恢复语义。
 */
void Sys_Critical_Exit(void) {
}

static const filter_ekf_config_t s_default_config = {
  .angle_process_noise_rad2 = 1.0e-4f,
  .bias_process_noise_radps2 = 1.0e-6f,
  .accel_measurement_noise_mps2_2 = 0.1f,
  .initial_angle_variance_rad2 = 1.0f,
  .initial_bias_variance_radps2 = 0.01f,
  .accel_norm_min_mps2 = 7.8f,
  .accel_norm_max_mps2 = 11.8f,
  .gyro_limit_radps = 17.45f,
  .bias_limit_radps = 1.0f,
  .min_dt_s = 0.001f,
  .max_dt_s = 0.05f,
};

/**
 * @brief 判断两个浮点标量是否在绝对容差内相等。
 * @param left 左操作数；与 right 使用相同物理单位。
 * @param right 右操作数；与 left 使用相同物理单位。
 * @param tolerance 非负绝对容差，单位与两个操作数相同。
 * @retval true fabsf(left - right) 不超过 tolerance。
 * @retval false 差值超出容差、任一操作数为 NaN，或 tolerance 为负。
 * @post 不修改输入，也不产生外部状态。
 * @note 测试辅助函数，不处理无穷值；仅供本测试程序比较数值结果。
 */
static bool floats_near(float left, float right, float tolerance) {
  return fabsf(left - right) <= tolerance;
}

/**
 * @brief 构造包含全部 IMU 通道的合成滤波输入。
 * @param time_ms 采样时间戳，单位 ms。
 * @return 按值返回 filter_sample_t；valid_mask 置位通道 3 至 8，加速度 Z 通道
 *         设为 9.80665 m/s^2，其余通道值为 0。
 * @post 不修改任何全局状态。
 * @note 该输入用于让 EKF 通过六轴完整性检查；其他通道没有填充真实传感器值。
 */
static filter_sample_t make_complete_input(uint32_t time_ms) {
  filter_sample_t input = {
    .valid_mask =
      (1u << 3) | (1u << 4) | (1u << 5) |
      (1u << 6) | (1u << 7) | (1u << 8),
    .time_ms = time_ms,
  };
  input.value[8] = 9.80665f;
  return input;
}

/**
 * @brief 验证 EKF 占位姿态输出与时间步钳制。
 * @post 成功返回时，首次完整输入产生占位姿态但清除姿态有效位，状态为
 *       invalid/placeholder、更新计数为 1、四元数为单位四元数，dt 等于最小周期
 *       0.001 s；120 ms 的第二次输入使 dt 变为 0.020 s。
 * @warning 任一 assert 失败都会终止测试进程，不返回可恢复的错误状态。
 * @note 测试对象位于栈上，返回后失效；EKF 核心仍为占位实现，结果不可用于控制。
 */
static void test_placeholder_and_time(void) {
  filter_ekf_t ekf;
  assert(Filter_Ekf_Init(&ekf, "ekf", &s_default_config));

  filter_sample_t input = make_complete_input(100u);
  filter_sample_t output;
  Filter_Base_Update(&ekf.filter_base, &input, &output);
  assert((output.valid_mask & 0x07u) == 0u);
  assert(floats_near(output.value[0], 0.0f, 1e-6f));
  assert(floats_near(output.value[1], 0.0f, 1e-6f));
  assert(floats_near(output.value[2], 0.0f, 1e-6f));

  filter_ekf_state_t state;
  Filter_Ekf_GetState(&ekf, &state);
  assert(!state.valid);
  assert(state.placeholder);
  assert(state.update_count == 1u);
  assert(floats_near(state.dt_s, s_default_config.min_dt_s, 1e-6f));
  assert(floats_near(state.quaternion[0], 1.0f, 1e-6f));

  input.time_ms = 120u;
  Filter_Base_Update(&ekf.filter_base, &input, &output);
  Filter_Ekf_GetState(&ekf, &state);
  assert(floats_near(state.dt_s, 0.02f, 1e-6f));
}

/**
 * @brief 验证不完整 IMU 输入不会发布有效姿态。
 * @post 成功返回时，仅含陀螺 Z 和加速度 Z 的输入使输出通道 0 至 2 的 valid 位
 *       清零，并令内部 state.valid 为 false。
 * @warning 任一 assert 失败都会终止测试进程，不返回可恢复的错误状态。
 * @note 测试对象位于栈上，返回后失效；失败帧不会访问外部传感器或硬件。
 */
static void test_incomplete_input(void) {
  filter_ekf_t ekf;
  assert(Filter_Ekf_Init(&ekf, "ekf", &s_default_config));

  filter_sample_t input = {
    .valid_mask = (1u << 5) | (1u << 8),
    .time_ms = 5u,
  };
  filter_sample_t output;
  Filter_Base_Update(&ekf.filter_base, &input, &output);
  assert((output.valid_mask & 0x07u) == 0u);

  filter_ekf_state_t state;
  Filter_Ekf_GetState(&ekf, &state);
  assert(!state.valid);
}

/**
 * @brief 验证 EKF 参数校验、延迟提交与默认值恢复。
 * @post 成功返回时，非法 dt 组合使初始化失败；合法测量噪声 0.2 在更新前仍保留
 *       旧配置 0.1，下一帧后变为 0.2；非法参数编号、零值、NaN 和越界组合均被
 *       拒绝；ResetParameters 后的下一帧恢复默认测量噪声 0.1。
 * @warning 任一 assert 失败都会终止测试进程，不返回可恢复的错误状态。
 * @note 参数采用 pending 机制，在帧边界生效；测试对象位于栈上，返回后失效。
 */
static void test_online_parameters(void) {
  filter_ekf_t ekf;
  assert(Filter_Ekf_Init(&ekf, "ekf", &s_default_config));

  filter_ekf_config_t invalid_config = s_default_config;
  invalid_config.max_dt_s = invalid_config.min_dt_s * 0.5f;
  assert(!Filter_Ekf_Init(&ekf, "ekf", &invalid_config));

  assert(Filter_Ekf_SetParameter(
    &ekf,
    FILTER_EKF_PARAM_ACCEL_MEASUREMENT_NOISE,
    0.2f
  ));
  assert(!Filter_Ekf_SetParameter(
    &ekf,
    FILTER_EKF_PARAM_COUNT,
    1.0f
  ));
  assert(!Filter_Ekf_SetParameter(
    &ekf,
    FILTER_EKF_PARAM_ACCEL_MEASUREMENT_NOISE,
    0.0f
  ));
  assert(!Filter_Ekf_SetParameter(
    &ekf,
    FILTER_EKF_PARAM_ACCEL_MEASUREMENT_NOISE,
    NAN
  ));
  assert(!Filter_Ekf_SetParameter(
    &ekf,
    FILTER_EKF_PARAM_ACCEL_NORM_MIN,
    12.0f
  ));

  filter_ekf_config_t before;
  Filter_Ekf_GetConfig(&ekf, &before);
  assert(floats_near(
    before.accel_measurement_noise_mps2_2,
    0.1f,
    1e-6f
  ));

  filter_sample_t input = make_complete_input(10u);
  filter_sample_t output;
  Filter_Base_Update(&ekf.filter_base, &input, &output);

  filter_ekf_config_t after;
  Filter_Ekf_GetConfig(&ekf, &after);
  assert(floats_near(
    after.accel_measurement_noise_mps2_2,
    0.2f,
    1e-6f
  ));

  Filter_Ekf_ResetParameters(&ekf);
  input.time_ms = 20u;
  Filter_Base_Update(&ekf.filter_base, &input, &output);
  Filter_Ekf_GetConfig(&ekf, &after);
  assert(floats_near(
    after.accel_measurement_noise_mps2_2,
    s_default_config.accel_measurement_noise_mps2_2,
    1e-6f
  ));
}

/**
 * @brief 依次运行 EKF 的三组契约测试并返回进程状态。
 * @return 固定返回 0；只有全部断言通过才会到达该返回语句。
 * @post 三个测试函数创建的栈对象均已失效，本文件不保留 EKF 全局状态。
 * @warning 任一 assert 失败都会终止进程，因此不会返回非零退出码。
 * @note 仅供 CTest 启动本测试可执行文件时调用。
 */
int main(void) {
  test_placeholder_and_time();
  test_incomplete_input();
  test_online_parameters();
  return 0;
}
