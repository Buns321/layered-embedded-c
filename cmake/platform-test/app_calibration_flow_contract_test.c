/**
 * @file app_calibration_flow_contract_test.c
 * @brief 快速通道任务一标定流程事件契约测试。
 * @ingroup tests
 */

#include "app_calibration_flow.h"
#include "system_log.h"
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static app_calibration_record_t s_active_record;
volatile uint32_t g_sys_log_fast_mask;

void App_CalibrationRecord_InitDefaults(
  app_calibration_record_t *record
) {
  assert(record);
  memset(record, 0, sizeof(*record));
  for (uint32_t axis = 0u; axis < APP_CALIBRATION_AXIS_COUNT; axis++) {
    record->accel_scale_ratio[axis] = 1.0f;
  }
}

const app_calibration_record_t *App_CalibrationRecord_GetActive(
  const app_calibration_record_store_t *store_h
) {
  (void)store_h;
  return &s_active_record;
}

bool App_CalibrationRecord_Apply(
  app_calibration_record_store_t *store_h,
  const app_calibration_record_t *record
) {
  (void)store_h;
  (void)record;
  return true;
}

bool App_CalibrationRecord_Commit(
  app_calibration_record_store_t *store_h,
  const app_calibration_record_t *record
) {
  (void)store_h;
  assert(record);
  s_active_record = *record;
  return true;
}

void Sys_Log_Write(
  sys_log_level_t level,
  const char *tag,
  const char *format,
  ...
) {
  (void)level;
  (void)tag;
  (void)format;
}

/**
 * @brief 构造静止且倒置的原始样本。
 * @return 三轴加速度 Z=-9.8 m/s²、角速度全零的样本。
 * @post 不修改全局状态。
 * @note 测试辅助函数。
 */
static imu_sample_t make_inverted_sample(void) {
  imu_sample_t sample = {
    .valid_mask =
      IMU_CHANNEL_ACCEL_X |
      IMU_CHANNEL_ACCEL_Y |
      IMU_CHANNEL_ACCEL_Z |
      IMU_CHANNEL_GYRO_X |
      IMU_CHANNEL_GYRO_Y |
      IMU_CHANNEL_GYRO_Z,
    .accel_mps2 = {0.0f, 0.0f, -9.80665f},
  };
  return sample;
}

/**
 * @brief 构造静止且正置的原始样本。
 * @return 三轴加速度 Z=+9.8 m/s²、角速度全零的样本。
 * @post 不修改全局状态。
 * @note 测试辅助函数。
 */
static imu_sample_t make_upright_sample(void) {
  imu_sample_t sample = {
    .valid_mask =
      IMU_CHANNEL_ACCEL_X |
      IMU_CHANNEL_ACCEL_Y |
      IMU_CHANNEL_ACCEL_Z |
      IMU_CHANNEL_GYRO_X |
      IMU_CHANNEL_GYRO_Y |
      IMU_CHANNEL_GYRO_Z,
    .accel_mps2 = {0.0f, 0.0f, 9.80665f},
  };
  return sample;
}

/**
 * @brief 构造指定安装面的静止样本。
 * @param x_mps2 X 轴加速度，单位 m/s²。
 * @param y_mps2 Y 轴加速度，单位 m/s²。
 * @param z_mps2 Z 轴加速度，单位 m/s²。
 * @return 六轴有效、角速度为零的静止样本。
 * @post 不修改全局状态。
 * @note 测试辅助函数。
 */
static imu_sample_t make_face_sample(
  float x_mps2,
  float y_mps2,
  float z_mps2
) {
  imu_sample_t sample = {
    .valid_mask =
      IMU_CHANNEL_ACCEL_X |
      IMU_CHANNEL_ACCEL_Y |
      IMU_CHANNEL_ACCEL_Z |
      IMU_CHANNEL_GYRO_X |
      IMU_CHANNEL_GYRO_Y |
      IMU_CHANNEL_GYRO_Z,
    .accel_mps2 = {x_mps2, y_mps2, z_mps2},
  };
  return sample;
}

/**
 * @brief 验证连续倒置静止三秒通过事件进入主程序。
 * @post 首次调用无事件，3000 ms 后返回 MAIN_ENTERED。
 * @note 记录持久化使用测试替身。
 */
static void test_initial_inverted_skip(void) {
  App_CalibrationRecord_InitDefaults(&s_active_record);
  app_calibration_record_store_t store = {0};
  app_calibration_flow_t flow;
  assert(App_CalibrationFlow_Init(&flow, &store));

  imu_sample_t raw = make_inverted_sample();
  uint32_t events = App_CalibrationFlow_Process(
    &flow,
    true,
    &raw,
    false,
    NULL,
    0u
  );
  assert(events == APP_CALIBRATION_EVENT_NONE);

  events = App_CalibrationFlow_Process(
    &flow,
    true,
    &raw,
    false,
    NULL,
    3000u
  );
  assert((events & APP_CALIBRATION_EVENT_MAIN_ENTERED) != 0u);
  assert(
    App_CalibrationFlow_GetState(&flow) ==
    APP_FAST_CHANNEL_STATE_MAIN
  );
}

/**
 * @brief 验证滤波后样本持续无效时六面流程仍按时退出。
 * @post 初始姿态和陀螺零偏完成后，无校正样本持续到 30 秒截止时间会返回
 *       MAIN_ENTERED，而不是无限停留在 WAIT_FACE。
 * @note 记录持久化使用测试替身。
 */
static void test_face_timeout_without_calibrated_sample(void) {
  App_CalibrationRecord_InitDefaults(&s_active_record);
  app_calibration_record_store_t store = {0};
  app_calibration_flow_t flow;
  assert(App_CalibrationFlow_Init(&flow, &store));

  imu_sample_t raw = make_upright_sample();
  uint32_t now_ms = 0u;
  assert(App_CalibrationFlow_Process(
    &flow,
    true,
    &raw,
    false,
    NULL,
    now_ms
  ) == APP_CALIBRATION_EVENT_NONE);

  now_ms = 3000u;
  assert(App_CalibrationFlow_Process(
    &flow,
    true,
    &raw,
    false,
    NULL,
    now_ms
  ) == APP_CALIBRATION_EVENT_NONE);
  assert(
    App_CalibrationFlow_GetState(&flow) ==
    APP_FAST_CHANNEL_STATE_GYRO_CALIBRATION
  );

  for (uint32_t sample_index = 0u; sample_index < 100u; sample_index++) {
    now_ms += 10u;
    uint32_t events = App_CalibrationFlow_Process(
      &flow,
      true,
      &raw,
      false,
      NULL,
      now_ms
    );
    if (sample_index + 1u < 100u) {
      assert(events == APP_CALIBRATION_EVENT_NONE);
    } else {
      assert(events == APP_CALIBRATION_EVENT_GYRO_READY);
    }
  }
  assert(
    App_CalibrationFlow_GetState(&flow) ==
    APP_FAST_CHANNEL_STATE_WAIT_FACE
  );
  assert(
    (App_CalibrationFlow_GetFaceMask(&flow) & APP_FACE_ALL) == 0u
  );

  now_ms += 30000u;
  uint32_t events = App_CalibrationFlow_Process(
    &flow,
    true,
    &raw,
    false,
    NULL,
    now_ms
  );
  assert((events & APP_CALIBRATION_EVENT_MAIN_ENTERED) != 0u);
  assert(
    App_CalibrationFlow_GetState(&flow) ==
    APP_FAST_CHANNEL_STATE_MAIN
  );
}

/**
 * @brief 验证六个安装面依次完成后进入主程序。
 * @post 每面稳定 1 秒并采样 100 点后提交；前五面产生 FACE_SAVED，第六面产生
 *       MAIN_ENTERED。
 * @note 记录持久化使用测试替身，不访问 Flash。
 */
static void test_complete_face_sequence(void) {
  App_CalibrationRecord_InitDefaults(&s_active_record);
  app_calibration_record_store_t store = {0};
  app_calibration_flow_t flow;
  assert(App_CalibrationFlow_Init(&flow, &store));

  imu_sample_t upright = make_upright_sample();
  uint32_t now_ms = 0u;
  assert(App_CalibrationFlow_Process(
    &flow,
    true,
    &upright,
    false,
    NULL,
    now_ms
  ) == APP_CALIBRATION_EVENT_NONE);

  now_ms = 3000u;
  assert(App_CalibrationFlow_Process(
    &flow,
    true,
    &upright,
    false,
    NULL,
    now_ms
  ) == APP_CALIBRATION_EVENT_NONE);

  for (uint32_t sample_index = 0u; sample_index < 100u; sample_index++) {
    now_ms += 10u;
    uint32_t events = App_CalibrationFlow_Process(
      &flow,
      true,
      &upright,
      false,
      NULL,
      now_ms
    );
    if (sample_index + 1u == 100u) {
      assert(events == APP_CALIBRATION_EVENT_GYRO_READY);
    } else {
      assert(events == APP_CALIBRATION_EVENT_NONE);
    }
  }

  static const float face_accel_mps2[6][3] = {
    {9.80665f, 0.0f, 0.0f},
    {-9.80665f, 0.0f, 0.0f},
    {0.0f, 9.80665f, 0.0f},
    {0.0f, -9.80665f, 0.0f},
    {0.0f, 0.0f, 9.80665f},
    {0.0f, 0.0f, -9.80665f},
  };

  for (uint32_t face_index = 0u; face_index < 6u; face_index++) {
    imu_sample_t sample = make_face_sample(
      face_accel_mps2[face_index][0],
      face_accel_mps2[face_index][1],
      face_accel_mps2[face_index][2]
    );
    assert(App_CalibrationFlow_Process(
      &flow,
      true,
      &sample,
      true,
      &sample,
      now_ms
    ) == APP_CALIBRATION_EVENT_NONE);

    now_ms += 1000u;
    assert(App_CalibrationFlow_Process(
      &flow,
      true,
      &sample,
      true,
      &sample,
      now_ms
    ) == APP_CALIBRATION_EVENT_NONE);

    uint32_t events = APP_CALIBRATION_EVENT_NONE;
    for (uint32_t sample_index = 0u; sample_index < 100u; sample_index++) {
      now_ms += 10u;
      events = App_CalibrationFlow_Process(
        &flow,
        true,
        &sample,
        true,
        &sample,
        now_ms
      );
      if (sample_index + 1u < 100u) {
        assert(events == APP_CALIBRATION_EVENT_NONE);
      }
    }

    if (face_index + 1u < 6u) {
      assert(events == APP_CALIBRATION_EVENT_FACE_SAVED);
    } else {
      assert((events & APP_CALIBRATION_EVENT_MAIN_ENTERED) != 0u);
    }
  }

  assert(
    App_CalibrationFlow_GetState(&flow) ==
    APP_FAST_CHANNEL_STATE_MAIN
  );
  assert(App_CalibrationFlow_GetFaceMask(&flow) == APP_FACE_ALL);
}

/**
 * @brief 运行标定流程契约测试。
 * @return 全部断言通过后返回 0。
 * @post 测试不保留外部资源。
 * @note 任一断言失败会终止进程。
 */
int main(void) {
  test_initial_inverted_skip();
  test_face_timeout_without_calibrated_sample();
  test_complete_face_sequence();
  return 0;
}
