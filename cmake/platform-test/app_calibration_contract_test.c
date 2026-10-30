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

/**
 * @brief 判断两个单精度浮点数是否在容差内相等。
 * @param left 比较左值；量纲应与 right 相同。
 * @param right 比较右值；量纲应与 left 相同。
 * @param tolerance 允许的绝对误差，量纲与输入相同；负值会使比较恒为 false。
 * @retval true 差值绝对值不大于 tolerance。
 * @retval false 差值超过容差，或任一输入为 NaN。
 * @post 只读取输入，不修改测试状态或全局数据。
 * @note 辅助函数不校验参数，也不使用断言；无穷值相减得到 NaN 时返回 false。
 */
static bool float_near(float left, float right, float tolerance) {
  return fabsf(left - right) <= tolerance;
}

/**
 * @brief 验证静止检测的加速度模长、角速度和门限切换。
 * @post 断言覆盖接近 1 g 的静止样本、低加速度拒绝、超过角速度上限拒绝，
 *       以及放宽角速度上限后同一未校准样本被接受的行为。
 * @note 调用真实标定实现；任一断言失败会终止进程，测试不访问传感器硬件。
 */
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

/**
 * @brief 验证六个轴向安装面和倾斜样本的识别结果。
 * @post 断言逐一确认 +X、-X、+Y、-Y、+Z、-Z 的位掩码，
 *       并确认三轴均为 6 m/s² 的对角样本因非主轴超过上限而失败。
 * @note 调用真实标定实现；任一断言失败会终止进程，不读写硬件。
 */
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

/**
 * @brief 验证静止与安装面检测拒绝非法门限。
 * @post 断言覆盖 NaN 加速度门限、上下限倒置、负角速度门限、
 *       NaN 主轴门限、负非主轴门限，以及非主轴门限等于主轴门限。
 * @note 所有非法输入都应返回 false；任一断言失败会终止进程，
 *       测试不检查失败时 face 输出是否保持不变。
 */
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

/**
 * @brief 验证单轴零偏、比例系数公式和输入范围检查。
 * @post 断言 10 m/s² 与 -9 m/s² 的样本得到 0.5 m/s² 零偏和
 *       2g/19 的比例系数，并确认跨度过小及非法轴号返回 false。
 * @note 调用真实标定实现；断言失败会终止进程，测试不涉及硬件。
 */
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

/**
 * @brief 运行本文件定义的契约测试并返回进程状态。
 * @retval 0 四个标定契约测试均通过。
 * @post 进程以成功状态退出，不保留动态资源或硬件句柄。
 * @warning 任一契约断言失败都会终止进程，因此失败时不会返回状态码。
 */
int main(void) {
  test_still_detection();
  test_face_detection();
  test_invalid_thresholds();
  test_axis_formula();
  return 0;
}
