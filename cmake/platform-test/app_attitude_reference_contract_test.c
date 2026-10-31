/**
 * @file app_attitude_reference_contract_test.c
 * @brief 快速通道任务一参考姿态模块契约测试。
 * @ingroup tests
 */

#include "app_attitude_reference.h"
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
 * @brief 验证初始化、直接姿态、陀螺积分和时间步。
 * @post 首帧使用标称周期，后续使用 10 ms；积分角按 deg/s 转 rad 推进。
 * @note 不访问硬件。
 */
static void test_reference_update(void) {
  app_attitude_reference_t reference;
  assert(!App_AttitudeReference_Init(NULL, 0.01f));
  assert(!App_AttitudeReference_Init(&reference, 0.0f));
  assert(App_AttitudeReference_Init(&reference, 0.01f));

  imu_sample_t sample = {
    .valid_mask =
      IMU_CHANNEL_ACCEL_X |
      IMU_CHANNEL_ACCEL_Y |
      IMU_CHANNEL_ACCEL_Z |
      IMU_CHANNEL_GYRO_X |
      IMU_CHANNEL_GYRO_Y |
      IMU_CHANNEL_GYRO_Z,
    .accel_mps2 = {1.0f, 2.0f, 3.0f},
    .gyro_dps = {0.0f, 0.0f, 10.0f},
  };
  App_AttitudeReference_Update(&reference, &sample, 100u);

  app_attitude_reference_snapshot_t snapshot;
  App_AttitudeReference_GetSnapshot(&reference, &snapshot);
  assert(near(snapshot.direct_roll_rad, atan2f(2.0f, 3.0f)));
  assert(near(snapshot.direct_pitch_rad, atan2f(-1.0f, sqrtf(13.0f))));
  assert(near(snapshot.integrated_yaw_rad, 0.1f * 0.01745329251994329577f));
  assert(near(snapshot.dt_s, 0.01f));

  App_AttitudeReference_Update(&reference, &sample, 110u);
  App_AttitudeReference_GetSnapshot(&reference, &snapshot);
  assert(near(snapshot.integrated_yaw_rad, 0.2f * 0.01745329251994329577f));
  assert(near(snapshot.dt_s, 0.01f));

  imu_sample_t partial_accel = sample;
  partial_accel.valid_mask = IMU_CHANNEL_ACCEL_Z;
  partial_accel.accel_mps2[0] = 9.80665f;
  partial_accel.accel_mps2[1] = 0.0f;
  partial_accel.accel_mps2[2] = 9.80665f;
  partial_accel.gyro_dps[2] = 0.0f;
  App_AttitudeReference_Update(&reference, &partial_accel, 120u);
  App_AttitudeReference_GetSnapshot(&reference, &snapshot);
  assert(near(snapshot.direct_roll_rad, atan2f(2.0f, 3.0f)));
  assert(near(snapshot.direct_pitch_rad, atan2f(-1.0f, sqrtf(13.0f))));
}

/**
 * @brief 运行参考姿态契约测试。
 * @return 全部断言通过后返回 0。
 * @post 测试不保留外部资源。
 * @note 任一断言失败会终止进程。
 */
int main(void) {
  test_reference_update();
  return 0;
}
