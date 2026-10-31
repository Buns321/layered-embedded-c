/**
 * @file app_imu_session.c
 * @brief 快速通道任务一 IMU 管线编排和一致快照实现。
 * @ingroup app
 */

#include "app_imu_session.h"
#include "app_attitude_reference.h"
#include "app_calibration_record.h"
#include "app_fast_channel_config.h"
#include "app_status_indicator.h"
#include "system_critical.h"
#include "system_log.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

/**
 * @brief IMU 会话私有状态。
 */
typedef struct app_imu_session_state_s {
  imu_base_t *imu_h;
  filter_ekf_t *ekf_h;
  app_calibration_record_store_t record_store;
  app_calibration_flow_t calibration_flow;
  app_status_indicator_t indicator;
  app_attitude_reference_t attitude_reference;
  app_fast_channel_snapshot_t snapshots[2];
  app_task_metrics_t metrics;
  volatile uint8_t active_snapshot;
  uint32_t snapshot_sequence;
  bool initialized;
} app_imu_session_state_t;

static app_imu_session_state_t s_session;

/**
 * @brief 发布一份新的 IMU 会话快照。
 * @param raw_valid 原始样本是否有效。
 * @param raw_sample 原始样本；raw_valid 为 true 时不得为 NULL。
 * @param calibrated_valid 校正样本是否有效。
 * @param calibrated_sample 校正样本；calibrated_valid 为 true 时不得为 NULL。
 * @pre 只由单一 IMU 任务调用。
 * @post 非活动双缓冲完整写入后通过短临界区切换活动索引。
 * @note 不分配内存，临界区只覆盖索引切换。
 */
static void app_imusession_publishsnapshot(
  bool raw_valid,
  const imu_sample_t *raw_sample,
  bool calibrated_valid,
  const imu_sample_t *calibrated_sample
) {
  assert(!raw_valid || raw_sample);
  assert(!calibrated_valid || calibrated_sample);

  uint8_t next_index = (uint8_t)(s_session.active_snapshot ^ 1u);
  app_fast_channel_snapshot_t *snapshot =
    &s_session.snapshots[next_index];
  memset(snapshot, 0, sizeof(*snapshot));

  if (raw_valid) {
    snapshot->raw_sample = *raw_sample;
    snapshot->raw_valid = true;
  }
  if (calibrated_valid) {
    snapshot->calibrated_sample = *calibrated_sample;
    snapshot->calibrated_valid = true;
  }
  Filter_Ekf_GetState(s_session.ekf_h, &snapshot->ekf_state);

  snapshot->calibration_state = App_CalibrationFlow_GetState(
    &s_session.calibration_flow
  );
  snapshot->face_mask = App_CalibrationFlow_GetFaceMask(
    &s_session.calibration_flow
  );
  snapshot->completed_axis_mask =
    App_CalibrationFlow_GetCompletedAxisMask(
      &s_session.calibration_flow
    );

  app_attitude_reference_snapshot_t reference;
  App_AttitudeReference_GetSnapshot(
    &s_session.attitude_reference,
    &reference
  );
  snapshot->direct_roll_rad = reference.direct_roll_rad;
  snapshot->direct_pitch_rad = reference.direct_pitch_rad;
  snapshot->integrated_roll_rad = reference.integrated_roll_rad;
  snapshot->integrated_pitch_rad = reference.integrated_pitch_rad;
  snapshot->integrated_yaw_rad = reference.integrated_yaw_rad;
  snapshot->dt_s = reference.dt_s;
  snapshot->flash_record_valid = App_CalibrationRecord_HasFlashRecord(
    &s_session.record_store
  );
  snapshot->flash_write_failed =
    App_CalibrationRecord_HasFlashFailure(&s_session.record_store);
  snapshot->metrics = s_session.metrics;
  snapshot->sequence = ++s_session.snapshot_sequence;

  Sys_Critical_Enter();
  s_session.active_snapshot = next_index;
  Sys_Critical_Exit();
}

/**
 * @brief 初始化 IMU 记录、标定、提示、参考姿态和快照。
 * @param imu_h 已初始化并可提交样本的统一 IMU 句柄，不得为 NULL。
 * @param ekf_h 已初始化的 EKF 对象，不得为 NULL。
 * @param calibration_filter_h 已初始化的六轴校正滤波器，不得为 NULL。
 * @retval PLATFORM_STATUS_OK 会话全部子模块已就绪。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 依赖指针为空。
 * @retval PLATFORM_STATUS_ERROR 默认记录或子模块初始化失败。
 * @pre 仅在调度器启动前、无并发使用者时调用。
 * @post 成功时进入初始姿态检查；Flash 缺失或损坏时使用默认记录继续。
 * @note 可能执行同步 Flash 扫描和擦除；不分配内存。
 */
platform_status_t App_ImuSession_Init(
  imu_base_t *imu_h,
  filter_ekf_t *ekf_h,
  filter_imu_calibration_t *calibration_filter_h
) {
  if (
    imu_h == NULL ||
    ekf_h == NULL ||
    calibration_filter_h == NULL
  ) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (s_session.initialized) {
    return PLATFORM_STATUS_OK;
  }

  memset(&s_session, 0, sizeof(s_session));
  s_session.imu_h = imu_h;
  s_session.ekf_h = ekf_h;

  App_StatusIndicator_Init(&s_session.indicator);
  if (!App_CalibrationRecord_Init(
        &s_session.record_store,
        calibration_filter_h
      )) {
    return PLATFORM_STATUS_ERROR;
  }
  (void)App_CalibrationRecord_Load(&s_session.record_store);

  if (!App_CalibrationFlow_Init(
        &s_session.calibration_flow,
        &s_session.record_store
      )) {
    return PLATFORM_STATUS_ERROR;
  }
  if (!App_AttitudeReference_Init(
        &s_session.attitude_reference,
        (float)APP_IMU_TASK_PERIOD_MS * 0.001f
      )) {
    return PLATFORM_STATUS_ERROR;
  }

  s_session.initialized = true;
  SYS_LOG_INFO(
    "APP",
    "imu session initialized flash=%u",
    App_CalibrationRecord_HasFlashRecord(&s_session.record_store)
      ? 1u
      : 0u
  );
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 推进一次 IMU 采样、标定流程、参考姿态和快照发布。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @pre 会话已成功初始化，且只由单一 IMU 任务调用。
 * @post 已驱动 IMU process、推进标定、更新 EKF 重置和提示、更新参考姿态并发布快照。
 * @note 可能执行阻塞 I2C 和 Flash 操作；不分配内存。
 */
void App_ImuSession_Process(uint32_t now_ms) {
  if (!s_session.initialized) {
    return;
  }

  Dev_IMU_Base_Process(s_session.imu_h, now_ms);

  imu_sample_t raw_sample = {0};
  imu_sample_t calibrated_sample = {0};
  bool raw_valid = Dev_IMU_Base_GetRawSample(
    s_session.imu_h,
    &raw_sample
  );
  bool calibrated_valid = Dev_IMU_Base_GetSample(
    s_session.imu_h,
    &calibrated_sample
  );

  uint32_t events = App_CalibrationFlow_Process(
    &s_session.calibration_flow,
    raw_valid,
    raw_valid ? &raw_sample : NULL,
    calibrated_valid,
    calibrated_valid ? &calibrated_sample : NULL,
    now_ms
  );

  if ((events & APP_CALIBRATION_EVENT_MAIN_ENTERED) != 0u) {
    Filter_Base_Reset(&s_session.ekf_h->filter_base);
  }
  App_StatusIndicator_Notify(&s_session.indicator, events, now_ms);

  if (calibrated_valid) {
    App_AttitudeReference_Update(
      &s_session.attitude_reference,
      &calibrated_sample,
      now_ms
    );
  }

  app_imusession_publishsnapshot(
    raw_valid,
    raw_valid ? &raw_sample : NULL,
    calibrated_valid,
    calibrated_valid ? &calibrated_sample : NULL
  );
  App_StatusIndicator_Process(
    &s_session.indicator,
    App_CalibrationFlow_GetState(&s_session.calibration_flow),
    now_ms
  );
}

/**
 * @brief 复制最近一次 IMU 会话快照。
 * @param snapshot 调用方输出存储，不得为 NULL。
 * @pre snapshot 可写且不与内部双缓冲重叠。
 * @post snapshot 获得一致的 IMU、EKF、标定、参考姿态和性能统计快照。
 * @note 使用短临界区复制；不分配内存，不得在 ISR 调用。
 */
void App_ImuSession_GetSnapshot(
  app_fast_channel_snapshot_t *snapshot
) {
  assert(snapshot);

  Sys_Critical_Enter();
  *snapshot = s_session.snapshots[s_session.active_snapshot];
  Sys_Critical_Exit();
}

/**
 * @brief 更新最近一次 IMU 任务性能统计。
 * @param metrics 非空性能统计；函数按值复制。
 * @pre 只由 ImuEkfTask 调用。
 * @post 后续快照包含新的性能统计。
 * @note 使用短临界区保护整体复制；不分配内存。
 */
void App_ImuSession_ReportMetrics(
  const app_task_metrics_t *metrics
) {
  assert(metrics);

  Sys_Critical_Enter();
  s_session.metrics = *metrics;
  Sys_Critical_Exit();
}
