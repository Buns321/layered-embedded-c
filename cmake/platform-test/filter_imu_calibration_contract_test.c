/**
 * @file filter_imu_calibration_contract_test.c
 * @brief IMU 六轴校正滤波器契约测试。
 * @ingroup tests
 */

#include "filter_base.h"
#include "filter_imu_calibration.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief IMU 校正契约测试使用的空临界区进入桩。
 * @pre 必须与 Sys_Critical_Exit 配对调用；本测试不在 ISR 中调用。
 * @post 函数体为空，不保存 PRIMASK、不关闭中断、不增加嵌套计数。
 * @warning 该桩不提供互斥或中断保护，只用于让校正滤波器公共层完成链接。
 * @note 本测试后续不依赖真实临界区；生产 System 实现才有中断/调度保护语义。
 */
void Sys_Critical_Enter(void) {
}

/**
 * @brief IMU 校正契约测试使用的空临界区退出桩。
 * @pre 之前已调用本文件的 Sys_Critical_Enter 桩；本函数不验证配对关系。
 * @post 函数体为空，不恢复 PRIMASK、不开启中断、不修改嵌套计数。
 * @warning 该桩不提供互斥或中断保护，只用于让校正滤波器公共层完成链接。
 * @note 必须与 Sys_Critical_Enter 成对调用；生产 System 实现才有恢复语义。
 */
void Sys_Critical_Exit(void) {
}

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
 * @brief 验证 IMU 校正的默认恒等参数保持输入不变。
 * @post 成功返回时，配置仅令三轴加速度比例为 1，零偏和陀螺偏置均为 0；
 *       输出通道 3、5、6、8 的数值、valid_mask 和 time_ms 与输入一致。
 * @warning 任一 assert 失败都会终止测试进程，不返回可恢复的错误状态。
 * @note 测试对象位于栈上，返回后失效；未置位的 IMU 通道不参与断言。
 */
static void test_identity_defaults(void) {
  const filter_imu_calibration_config_t config = {
    .accel_scale = {1.0f, 1.0f, 1.0f},
  };
  filter_imu_calibration_t calibration;
  assert(Filter_ImuCalibration_Init(&calibration, "cal", &config));

  filter_sample_t input = {
    .valid_mask =
      (1u << 3) | (1u << 5) | (1u << 6) | (1u << 8),
    .time_ms = 10u,
  };
  input.value[3] = 1.25f;
  input.value[5] = -2.5f;
  input.value[6] = 0.5f;
  input.value[8] = 9.80665f;

  filter_sample_t output;
  Filter_Base_Update(&calibration.filter_base, &input, &output);
  assert(floats_near(output.value[3], input.value[3], 1e-6f));
  assert(floats_near(output.value[5], input.value[5], 1e-6f));
  assert(floats_near(output.value[6], input.value[6], 1e-6f));
  assert(floats_near(output.value[8], input.value[8], 1e-6f));
  assert(output.valid_mask == input.valid_mask);
  assert(output.time_ms == input.time_ms);
}

/**
 * @brief 验证 IMU 零偏、比例校正和帧计数复位。
 * @post 成功返回时，陀螺通道按 raw-bias 校正，加速度通道按
 *       (raw-offset)*scale 校正，所有输出与预期值在 1e-6 内一致；
 *       corrected_frames 在更新后为 1，执行 Filter_Base_Reset 后为 0。
 * @warning 任一 assert 失败都会终止测试进程，不返回可恢复的错误状态。
 * @note 测试对象位于栈上，返回后失效；校正过程只读写内存样本，不访问传感器。
 */
static void test_correction_and_update(void) {
  const filter_imu_calibration_config_t initial = {
    .accel_scale = {1.0f, 1.0f, 1.0f},
  };
  filter_imu_calibration_t calibration;
  assert(Filter_ImuCalibration_Init(
    &calibration,
    "cal",
    &initial
  ));

  const filter_imu_calibration_config_t corrected = {
    .accel_offset_mps2 = {0.5f, -1.0f, 0.25f},
    .accel_scale = {2.0f, 0.5f, 1.25f},
    .gyro_bias_dps = {1.0f, -2.0f, 0.5f},
  };
  assert(Filter_ImuCalibration_SetConfig(&calibration, &corrected));

  filter_sample_t input = {
    .valid_mask =
      (1u << 3) | (1u << 4) | (1u << 5) |
      (1u << 6) | (1u << 7) | (1u << 8),
    .time_ms = 20u,
  };
  input.value[3] = 2.0f;
  input.value[4] = 1.0f;
  input.value[5] = -1.5f;
  input.value[6] = 1.5f;
  input.value[7] = 3.0f;
  input.value[8] = 9.0f;

  filter_sample_t output;
  Filter_Base_Update(&calibration.filter_base, &input, &output);

  assert(floats_near(output.value[3], 1.0f, 1e-6f));
  assert(floats_near(output.value[4], 3.0f, 1e-6f));
  assert(floats_near(output.value[5], -2.0f, 1e-6f));
  assert(floats_near(output.value[6], 2.0f, 1e-6f));
  assert(floats_near(output.value[7], 2.0f, 1e-6f));
  assert(floats_near(output.value[8], 10.9375f, 1e-6f));
  assert(calibration.corrected_frames == 1u);

  Filter_Base_Reset(&calibration.filter_base);
  assert(calibration.corrected_frames == 0u);
}

/**
 * @brief 验证空配置、非法比例和超限陀螺偏置被拒绝。
 * @post 成功返回时，NULL 配置初始化失败，零比例和 721 deg/s 偏置的
 *       SetConfig 调用均返回 false；失败后读回的比例仍为 1，说明原配置未改变。
 * @warning 任一 assert 失败都会终止测试进程，不返回可恢复的错误状态。
 * @note 测试对象位于栈上，返回后失效；非法配置不会写入全局状态或硬件。
 */
static void test_invalid_configuration(void) {
  filter_imu_calibration_t calibration;
  assert(!Filter_ImuCalibration_Init(&calibration, "cal", NULL));

  filter_imu_calibration_config_t config = {
    .accel_scale = {1.0f, 1.0f, 1.0f},
  };
  assert(Filter_ImuCalibration_Init(&calibration, "cal", &config));

  config.accel_scale[1] = 0.0f;
  assert(!Filter_ImuCalibration_SetConfig(&calibration, &config));

  config.accel_scale[1] = 1.0f;
  config.gyro_bias_dps[2] = 721.0f;
  assert(!Filter_ImuCalibration_SetConfig(&calibration, &config));

  filter_imu_calibration_config_t readback;
  Filter_ImuCalibration_GetConfig(&calibration, &readback);
  assert(floats_near(readback.accel_scale[1], 1.0f, 1e-6f));
}

/**
 * @brief 依次运行 IMU 校正的三组契约测试并返回进程状态。
 * @return 固定返回 0；只有全部断言通过才会到达该返回语句。
 * @post 三个测试函数创建的栈对象均已失效，本文件不保留校正器全局状态。
 * @warning 任一 assert 失败都会终止进程，因此不会返回非零退出码。
 * @note 仅供 CTest 启动本测试可执行文件时调用。
 */
int main(void) {
  test_identity_defaults();
  test_correction_and_update();
  test_invalid_configuration();
  return 0;
}
