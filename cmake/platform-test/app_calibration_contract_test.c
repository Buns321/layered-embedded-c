/**
 * @file app_calibration_contract_test.c
 * @brief 速通任务一六面识别与加速度标定公式契约测试。
 * @ingroup tests
 */

#include "app_calibration.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

static const app_calibration_thresholds_t s_thresholds = {
  .still_accel_min_mps2 = 7.8f,
  .still_accel_max_mps2 = 11.8f,
  .still_gyro_max_dps = 3.0f,
  .face_dominant_min_mps2 = 6.0f,
  .face_cross_max_mps2 = 5.0f,
};

static bool float_near(float left, float right, float tolerance) {
  return fabsf(left - right) <= tolerance;
}

static void test_still_detection(void) {
  const float accel_up[3] = {0.1f, -0.2f, 9.80665f};
  const float gyro_still[3] = {0.2f, -0.1f, 0.3f};
  assert(App_Calibration_IsStill(accel_up, gyro_still, &s_thresholds));

  const float accel_low[3] = {0.0f, 0.0f, 5.0f};
  assert(!App_Calibration_IsStill(accel_low, gyro_still, &s_thresholds));

  const float gyro_moving[3] = {0.0f, 0.0f, 3.1f};
  assert(!App_Calibration_IsStill(accel_up, gyro_moving, &s_thresholds));

  app_calibration_thresholds_t raw_thresholds = s_thresholds;
  raw_thresholds.still_gyro_max_dps = 10.0f;
  const float gyro_uncalibrated[3] = {-4.7f, -1.7f, 0.0f};
  assert(App_Calibration_IsStill(
    accel_up,
    gyro_uncalibrated,
    &raw_thresholds
  ));
  assert(!App_Calibration_IsStill(
    accel_up,
    gyro_uncalibrated,
    &s_thresholds
  ));
}

static void test_face_detection(void) {
  const float faces[6][3] = {
    {9.80665f, 0.0f, 0.0f},
    {-9.80665f, 0.0f, 0.0f},
    {0.0f, 9.80665f, 0.0f},
    {0.0f, -9.80665f, 0.0f},
    {0.0f, 0.0f, 9.80665f},
    {0.0f, 0.0f, -9.80665f},
  };
  const app_face_mask_t expected[6] = {
    APP_FACE_X_POSITIVE,
    APP_FACE_X_NEGATIVE,
    APP_FACE_Y_POSITIVE,
    APP_FACE_Y_NEGATIVE,
    APP_FACE_Z_POSITIVE,
    APP_FACE_Z_NEGATIVE,
  };

  for (uint8_t index = 0u; index < 6u; index++) {
    app_face_mask_t face = 0;
    assert(App_Calibration_DetectFace(
      faces[index],
      &s_thresholds,
      &face
    ));
    assert(face == expected[index]);
  }

  const float diagonal[3] = {6.0f, 6.0f, 6.0f};
  app_face_mask_t face = 0;
  assert(!App_Calibration_DetectFace(
    diagonal,
    &s_thresholds,
    &face
  ));
}

static void test_invalid_thresholds(void) {
  const float accel_up[3] = {0.0f, 0.0f, 9.80665f};
  const float gyro_still[3] = {0.0f, 0.0f, 0.0f};
  app_calibration_thresholds_t thresholds = s_thresholds;

  thresholds.still_accel_min_mps2 = NAN;
  assert(!App_Calibration_IsStill(accel_up, gyro_still, &thresholds));

  thresholds = s_thresholds;
  thresholds.still_accel_min_mps2 = 12.0f;
  thresholds.still_accel_max_mps2 = 8.0f;
  assert(!App_Calibration_IsStill(accel_up, gyro_still, &thresholds));

  thresholds = s_thresholds;
  thresholds.still_gyro_max_dps = -1.0f;
  assert(!App_Calibration_IsStill(accel_up, gyro_still, &thresholds));

  app_face_mask_t face = 0;
  thresholds = s_thresholds;
  thresholds.face_dominant_min_mps2 = NAN;
  assert(!App_Calibration_DetectFace(accel_up, &thresholds, &face));

  thresholds = s_thresholds;
  thresholds.face_cross_max_mps2 = -1.0f;
  assert(!App_Calibration_DetectFace(accel_up, &thresholds, &face));

  thresholds = s_thresholds;
  thresholds.face_cross_max_mps2 =
    thresholds.face_dominant_min_mps2;
  assert(!App_Calibration_DetectFace(accel_up, &thresholds, &face));
}

static void test_axis_formula(void) {
  const float positive[3] = {10.0f, 0.0f, 0.0f};
  const float negative[3] = {-9.0f, 0.0f, 0.0f};
  float offset = 0.0f;
  float scale = 0.0f;

  assert(App_Calibration_ComputeAccelAxis(
    positive,
    negative,
    0u,
    9.80665f,
    4.0f,
    0.1f,
    10.0f,
    &offset,
    &scale
  ));
  assert(float_near(offset, 0.5f, 1e-6f));
  assert(float_near(scale, (2.0f * 9.80665f) / 19.0f, 1e-6f));

  const float bad_negative[3] = {7.0f, 0.0f, 0.0f};
  assert(!App_Calibration_ComputeAccelAxis(
    positive,
    bad_negative,
    0u,
    9.80665f,
    4.0f,
    0.1f,
    10.0f,
    &offset,
    &scale
  ));

  assert(!App_Calibration_ComputeAccelAxis(
    positive,
    negative,
    3u,
    9.80665f,
    4.0f,
    0.1f,
    10.0f,
    &offset,
    &scale
  ));
}

int main(void) {
  test_still_detection();
  test_face_detection();
  test_invalid_thresholds();
  test_axis_formula();
  return 0;
}
