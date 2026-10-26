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
#include <stdint.h>

void Sys_Critical_Enter(void) {
}

void Sys_Critical_Exit(void) {
}

static bool floats_near(float left, float right, float tolerance) {
  return fabsf(left - right) <= tolerance;
}

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

int main(void) {
  test_identity_defaults();
  test_correction_and_update();
  test_invalid_configuration();
  return 0;
}
