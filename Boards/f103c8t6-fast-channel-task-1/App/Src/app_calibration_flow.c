/**
 * @file app_calibration_flow.c
 * @brief 快速通道任务一 IMU 标定状态机实现。
 * @ingroup app
 */

#include "app_calibration_flow.h"
#include "app_fast_channel_config.h"
#include "system_log.h"
#include "system_math.h"
#include "system_time_window.h"
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#define APP_CALIBRATION_DEG_TO_RAD 0.01745329251994329577f
#define APP_CALIBRATION_FACE_COUNT 6u
#define APP_CALIBRATION_AXIS_X 0u
#define APP_CALIBRATION_AXIS_Y 1u
#define APP_CALIBRATION_AXIS_Z 2u

static const app_calibration_thresholds_t s_raw_thresholds = {
  .still_accel_min_mps2 = APP_STILL_ACCEL_MIN_MPS2,
  .still_accel_max_mps2 = APP_STILL_ACCEL_MAX_MPS2,
  .still_gyro_max_dps = APP_RAW_STILL_GYRO_MAX_DPS,
  .face_dominant_min_mps2 = APP_FACE_DOMINANT_MIN_MPS2,
  .face_cross_max_mps2 = APP_FACE_CROSS_MAX_MPS2,
};

static const app_calibration_thresholds_t s_calibrated_thresholds = {
  .still_accel_min_mps2 = APP_STILL_ACCEL_MIN_MPS2,
  .still_accel_max_mps2 = APP_STILL_ACCEL_MAX_MPS2,
  .still_gyro_max_dps = APP_STILL_GYRO_MAX_DPS,
  .face_dominant_min_mps2 = APP_FACE_DOMINANT_MIN_MPS2,
  .face_cross_max_mps2 = APP_FACE_CROSS_MAX_MPS2,
};

/**
 * @brief 判断原始样本是否满足未校正静止门限。
 * @param sample 非空原始样本，加速度单位 m/s²、角速度单位 deg/s。
 * @return 满足静止门限时返回 true。
 * @pre sample 非 NULL 且至少包含三轴加速度和角速度。
 * @post 不修改样本。
 * @note 内部辅助函数，不分配内存。
 */
static bool app_calibrationflow_rawisstill(
  const imu_sample_t *sample
) {
  assert(sample);
  return App_Calibration_IsStill(
    sample->accel_mps2,
    sample->gyro_dps,
    &s_raw_thresholds
  );
}

/**
 * @brief 检测原始样本对应的唯一安装面。
 * @param sample 非空原始样本，使用三轴加速度，单位 m/s²。
 * @return 单个面位掩码；无法识别时返回 0。
 * @pre sample 非 NULL。
 * @post 不修改样本或状态。
 * @note 内部辅助函数，不分配内存。
 */
static uint32_t app_calibrationflow_detectface(
  const imu_sample_t *sample
) {
  assert(sample);
  app_face_mask_t face = 0;
  return App_Calibration_DetectFace(
    sample->accel_mps2,
    &s_raw_thresholds,
    &face
  )
    ? (uint32_t)face
    : 0u;
}

/**
 * @brief 把安装面位映射为面数组下标。
 * @param face 单个安装面位。
 * @return X+..Z- 映射为 0..5，非法值返回 UINT8_MAX。
 * @pre 无。
 * @post 不修改状态。
 * @note 内部辅助函数，不分配内存。
 */
static uint8_t app_calibrationflow_faceindex(uint32_t face) {
  switch (face) {
    case APP_FACE_X_POSITIVE: return 0u;
    case APP_FACE_X_NEGATIVE: return 1u;
    case APP_FACE_Y_POSITIVE: return 2u;
    case APP_FACE_Y_NEGATIVE: return 3u;
    case APP_FACE_Z_POSITIVE: return 4u;
    case APP_FACE_Z_NEGATIVE: return 5u;
    default: return UINT8_MAX;
  }
}

/**
 * @brief 把安装面映射为对应轴下标。
 * @param face 单个安装面位。
 * @return X 面为 0，Y 面为 1，其他为 2。
 * @pre face 已由 faceindex 验证。
 * @post 不修改状态。
 * @note 内部辅助函数，不分配内存。
 */
static uint8_t app_calibrationflow_faceaxis(uint32_t face) {
  if (
    face == APP_FACE_X_POSITIVE ||
    face == APP_FACE_X_NEGATIVE
  ) {
    return APP_CALIBRATION_AXIS_X;
  }
  if (
    face == APP_FACE_Y_POSITIVE ||
    face == APP_FACE_Y_NEGATIVE
  ) {
    return APP_CALIBRATION_AXIS_Y;
  }
  return APP_CALIBRATION_AXIS_Z;
}

/**
 * @brief 取得指定安装面的相对面。
 * @param face 单个安装面位。
 * @return 相对面位；非法输入返回 0。
 * @pre 无。
 * @post 不修改状态。
 * @note 内部辅助函数，不分配内存。
 */
static uint32_t app_calibrationflow_oppositeface(uint32_t face) {
  switch (face) {
    case APP_FACE_X_POSITIVE: return APP_FACE_X_NEGATIVE;
    case APP_FACE_X_NEGATIVE: return APP_FACE_X_POSITIVE;
    case APP_FACE_Y_POSITIVE: return APP_FACE_Y_NEGATIVE;
    case APP_FACE_Y_NEGATIVE: return APP_FACE_Y_POSITIVE;
    case APP_FACE_Z_POSITIVE: return APP_FACE_Z_NEGATIVE;
    case APP_FACE_Z_NEGATIVE: return APP_FACE_Z_POSITIVE;
    default: return 0u;
  }
}

/**
 * @brief 接受当前安装面均值并更新工作记录。
 * @param flow_h 非空流程。
 * @param face 单个合法安装面位。
 * @param accel_mean_mps2 三轴平均加速度，单位 m/s²。
 * @retval true 面已接受，工作记录已更新。
 * @retval false 面非法、相对面跨度不足或比例越界。
 * @pre 已完成该面样本平均，且 flow_h 由单一任务持有。
 * @post 成功时 face_values_mps2、面掩码和工作记录会更新；不写 Flash。
 * @note 内部辅助函数，不分配内存。
 */
static bool app_calibrationflow_acceptface(
  app_calibration_flow_t *flow_h,
  uint32_t face,
  const float accel_mean_mps2[APP_CALIBRATION_AXIS_COUNT]
) {
  assert(flow_h && accel_mean_mps2);

  uint8_t face_index = app_calibrationflow_faceindex(face);
  uint8_t axis = app_calibrationflow_faceaxis(face);
  if (face_index == UINT8_MAX) {
    return false;
  }

  uint32_t opposite_face = app_calibrationflow_oppositeface(face);
  float offset_mps2 = flow_h->working_record.accel_offset_mps2[axis];
  float scale_ratio =
    flow_h->working_record.accel_scale_ratio[axis];
  if ((flow_h->face_mask & opposite_face) != 0u) {
    uint8_t opposite_index = app_calibrationflow_faceindex(opposite_face);
    bool current_is_positive =
      face == APP_FACE_X_POSITIVE ||
      face == APP_FACE_Y_POSITIVE ||
      face == APP_FACE_Z_POSITIVE;

    const float *positive_mps2 = current_is_positive
      ? accel_mean_mps2
      : flow_h->face_values_mps2[opposite_index];
    const float *negative_mps2 = current_is_positive
      ? flow_h->face_values_mps2[opposite_index]
      : accel_mean_mps2;
    if (!App_Calibration_ComputeAccelAxis(
          positive_mps2,
          negative_mps2,
          axis,
          APP_GRAVITY_MPS2,
          APP_ACCEL_CALIBRATION_MIN_SPAN_MPS2,
          APP_ACCEL_SCALE_MIN,
          APP_ACCEL_SCALE_MAX,
          &offset_mps2,
          &scale_ratio
        )) {
      return false;
    }
    flow_h->completed_axis_mask |= (uint32_t)1u << axis;
  }

  for (
    uint8_t component = 0u;
    component < APP_CALIBRATION_AXIS_COUNT;
    component++
  ) {
    flow_h->face_values_mps2[face_index][component] =
      accel_mean_mps2[component];
  }
  flow_h->face_mask |= face;
  flow_h->working_record.face_mask = flow_h->face_mask;
  flow_h->working_record.completed_axis_mask =
    flow_h->completed_axis_mask;
  flow_h->working_record.accel_offset_mps2[axis] = offset_mps2;
  flow_h->working_record.accel_scale_ratio[axis] = scale_ratio;
  return true;
}

/**
 * @brief 进入陀螺零偏采集状态并清空累计器。
 * @param flow_h 非空流程。
 * @param now_ms 状态起点，单位 ms。
 * @pre 单任务调用。
 * @post 状态为 GYRO_CALIBRATION，采样计数和累计量清零。
 * @note 内部辅助函数，不分配内存。
 */
static void app_calibrationflow_entergyro(
  app_calibration_flow_t *flow_h,
  uint32_t now_ms
) {
  flow_h->state = APP_FAST_CHANNEL_STATE_GYRO_CALIBRATION;
  flow_h->state_started_ms = now_ms;
  flow_h->gyro_sample_count = 0u;
  memset(flow_h->gyro_sum_radps, 0, sizeof(flow_h->gyro_sum_radps));
  memset(
    flow_h->gyro_sum_sq_radps2,
    0,
    sizeof(flow_h->gyro_sum_sq_radps2)
  );
}

/**
 * @brief 进入主运行状态并恢复最近有效活动记录。
 * @param flow_h 非空流程。
 * @param now_ms 状态起点，单位 ms。
 * @param reason 日志原因，不得为 NULL。
 * @return MAIN_ENTERED 事件；重复调用返回 NONE。
 * @pre 单任务调用。
 * @post 工作记录和面掩码恢复为活动记录，状态进入 MAIN。
 * @note 内部辅助函数，不分配内存；EKF 重置由会话处理。
 */
static uint32_t app_calibrationflow_entermain(
  app_calibration_flow_t *flow_h,
  uint32_t now_ms,
  const char *reason
) {
  assert(flow_h && reason && flow_h->record_store_h);
  if (flow_h->state == APP_FAST_CHANNEL_STATE_MAIN) {
    return APP_CALIBRATION_EVENT_NONE;
  }

  const app_calibration_record_t *active =
    App_CalibrationRecord_GetActive(flow_h->record_store_h);
  flow_h->working_record = *active;
  flow_h->face_mask = active->face_mask;
  flow_h->completed_axis_mask = active->completed_axis_mask;
  (void)App_CalibrationRecord_Apply(flow_h->record_store_h, active);
  flow_h->state = APP_FAST_CHANNEL_STATE_MAIN;
  flow_h->state_started_ms = now_ms;
  flow_h->main_announced = true;
  SYS_LOG_INFO("APP", "main started: %s", reason);
  return APP_CALIBRATION_EVENT_MAIN_ENTERED;
}

/**
 * @brief 推进初始姿态确认。
 * @param flow_h 非空流程。
 * @param raw_sample 非空原始样本。
 * @param now_ms 当前时间，单位 ms。
 * @return 产生的事件位。
 * @pre 单任务调用。
 * @post 连续倒置静止 3 秒进入 MAIN，否则满 3 秒进入陀螺零偏采集。
 * @note 内部辅助函数，不分配内存。
 */
static uint32_t app_calibrationflow_processinitial(
  app_calibration_flow_t *flow_h,
  const imu_sample_t *raw_sample,
  uint32_t now_ms
) {
  bool inverted =
    app_calibrationflow_rawisstill(raw_sample) &&
    raw_sample->accel_mps2[APP_CALIBRATION_AXIS_Z] <=
      APP_INVERTED_AZ_MAX_MPS2;

  if (inverted) {
    if (!flow_h->inverted_tracking) {
      flow_h->inverted_tracking = true;
      flow_h->inverted_since_ms = now_ms;
    }
  } else {
    flow_h->inverted_tracking = false;
  }

  if (
    flow_h->inverted_tracking &&
    Sys_Time_HasElapsedMs(
      now_ms,
      flow_h->inverted_since_ms,
      APP_INITIAL_POSE_CHECK_MS
    )
  ) {
    return app_calibrationflow_entermain(
      flow_h,
      now_ms,
      "initial -Z skip"
    );
  }

  if (
    Sys_Time_HasElapsedMs(
      now_ms,
      flow_h->state_started_ms,
      APP_INITIAL_POSE_CHECK_MS
    )
  ) {
    app_calibrationflow_entergyro(flow_h, now_ms);
  }
  return APP_CALIBRATION_EVENT_NONE;
}

/**
 * @brief 推进陀螺零偏采集。
 * @param flow_h 非空流程。
 * @param raw_sample 非空原始样本。
 * @param now_ms 当前时间，单位 ms。
 * @return GYRO_READY、MAIN_ENTERED 或 NONE。
 * @pre 单任务调用。
 * @post 100 个静止样本标准差合格后写入工作零偏并进入 WAIT_FACE。
 * @note 内部辅助函数，不分配内存。
 */
static uint32_t app_calibrationflow_processgyro(
  app_calibration_flow_t *flow_h,
  const imu_sample_t *raw_sample,
  uint32_t now_ms
) {
  if (Sys_Time_HasElapsedMs(
        now_ms,
        flow_h->state_started_ms,
        APP_CALIBRATION_FACE_TIMEOUT_MS
      )) {
    return app_calibrationflow_entermain(
      flow_h,
      now_ms,
      "gyro calibration timeout"
    );
  }

  if (!app_calibrationflow_rawisstill(raw_sample)) {
    flow_h->gyro_sample_count = 0u;
    memset(flow_h->gyro_sum_radps, 0, sizeof(flow_h->gyro_sum_radps));
    memset(
      flow_h->gyro_sum_sq_radps2,
      0,
      sizeof(flow_h->gyro_sum_sq_radps2)
    );
    return APP_CALIBRATION_EVENT_NONE;
  }

  for (
    uint8_t axis = 0u;
    axis < APP_CALIBRATION_AXIS_COUNT;
    axis++
  ) {
    float value_radps =
      raw_sample->gyro_dps[axis] * APP_CALIBRATION_DEG_TO_RAD;
    flow_h->gyro_sum_radps[axis] += value_radps;
    flow_h->gyro_sum_sq_radps2[axis] += value_radps * value_radps;
  }
  flow_h->gyro_sample_count++;
  if (flow_h->gyro_sample_count < APP_GYRO_CALIBRATION_SAMPLES) {
    return APP_CALIBRATION_EVENT_NONE;
  }

  float count = (float)flow_h->gyro_sample_count;
  for (
    uint8_t axis = 0u;
    axis < APP_CALIBRATION_AXIS_COUNT;
    axis++
  ) {
    float mean_radps = flow_h->gyro_sum_radps[axis] / count;
    float variance =
      flow_h->gyro_sum_sq_radps2[axis] / count -
      mean_radps * mean_radps;
    if (variance < 0.0f) {
      variance = 0.0f;
    }
    if (
      sqrtf(variance) >
      APP_GYRO_CALIBRATION_MAX_STD_DPS * APP_CALIBRATION_DEG_TO_RAD
    ) {
      flow_h->gyro_sample_count = 0u;
      memset(flow_h->gyro_sum_radps, 0, sizeof(flow_h->gyro_sum_radps));
      memset(
        flow_h->gyro_sum_sq_radps2,
        0,
        sizeof(flow_h->gyro_sum_sq_radps2)
      );
      return APP_CALIBRATION_EVENT_NONE;
    }
    flow_h->working_record.gyro_bias_radps[axis] = mean_radps;
  }

  (void)App_CalibrationRecord_Apply(
    flow_h->record_store_h,
    &flow_h->working_record
  );
  flow_h->state = APP_FAST_CHANNEL_STATE_WAIT_FACE;
  flow_h->state_started_ms = now_ms;
  flow_h->face_deadline_ms = now_ms + APP_CALIBRATION_FACE_TIMEOUT_MS;
  SYS_LOG_INFO("CAL", "gyro bias calibrated");
  return APP_CALIBRATION_EVENT_GYRO_READY;
}

/**
 * @brief 清空当前安装面搜索状态。
 * @param flow_h 非空流程。
 * @param now_ms 新一轮窗口起点，单位 ms。
 * @pre 单任务调用。
 * @post 状态回到 WAIT_FACE，候选面和单面采样清零，截止时间延后 30 秒。
 * @note 内部辅助函数，不分配内存。
 */
static void app_calibrationflow_resetface(
  app_calibration_flow_t *flow_h,
  uint32_t now_ms
) {
  flow_h->state = APP_FAST_CHANNEL_STATE_WAIT_FACE;
  flow_h->state_started_ms = now_ms;
  flow_h->face_sample_count = 0u;
  flow_h->candidate_face = 0u;
  flow_h->face_deadline_ms = now_ms + APP_CALIBRATION_FACE_TIMEOUT_MS;
}

/**
 * @brief 推进安装面识别、稳定和采样。
 * @param flow_h 非空流程。
 * @param raw_sample 非空原始样本。
 * @param calibrated_sample 非空校正样本。
 * @param now_ms 当前时间，单位 ms。
 * @return 面保存、拒绝、提交失败、进入主程序或 NONE。
 * @pre 单任务调用。
 * @post 状态按 WAIT_FACE、STABILIZE_FACE、SAMPLE_FACE 推进；每面完成后提交一次 Flash。
 * @note 内部辅助函数，可能执行阻塞 Flash 操作，不分配内存。
 */
static uint32_t app_calibrationflow_processface(
  app_calibration_flow_t *flow_h,
  const imu_sample_t *raw_sample,
  const imu_sample_t *calibrated_sample,
  uint32_t now_ms
) {
  if (Sys_Time_HasReachedMs(now_ms, flow_h->face_deadline_ms)) {
    return app_calibrationflow_entermain(
      flow_h,
      now_ms,
      "face calibration timeout"
    );
  }

  uint32_t detected_face = app_calibrationflow_detectface(raw_sample);
  bool still = App_Calibration_IsStill(
    raw_sample->accel_mps2,
    calibrated_sample->gyro_dps,
    &s_calibrated_thresholds
  );

  switch (flow_h->state) {
    case APP_FAST_CHANNEL_STATE_WAIT_FACE:
      if (
        detected_face == 0u ||
        (flow_h->face_mask & detected_face) != 0u ||
        !still
      ) {
        return APP_CALIBRATION_EVENT_NONE;
      }
      flow_h->candidate_face = detected_face;
      flow_h->stable_started_ms = now_ms;
      flow_h->state = APP_FAST_CHANNEL_STATE_STABILIZE_FACE;
      return APP_CALIBRATION_EVENT_NONE;

    case APP_FAST_CHANNEL_STATE_STABILIZE_FACE:
      if (detected_face != flow_h->candidate_face || !still) {
        flow_h->state = APP_FAST_CHANNEL_STATE_WAIT_FACE;
        return APP_CALIBRATION_EVENT_NONE;
      }
      if (Sys_Time_HasElapsedMs(
            now_ms,
            flow_h->stable_started_ms,
            APP_FACE_STABLE_MS
          )) {
        flow_h->face_sample_count = 0u;
        memset(
          flow_h->face_sample_sum_mps2,
          0,
          sizeof(flow_h->face_sample_sum_mps2)
        );
        flow_h->state = APP_FAST_CHANNEL_STATE_SAMPLE_FACE;
      }
      return APP_CALIBRATION_EVENT_NONE;

    case APP_FAST_CHANNEL_STATE_SAMPLE_FACE:
      if (detected_face != flow_h->candidate_face || !still) {
        flow_h->state = APP_FAST_CHANNEL_STATE_WAIT_FACE;
        return APP_CALIBRATION_EVENT_NONE;
      }
      for (
        uint8_t axis = 0u;
        axis < APP_CALIBRATION_AXIS_COUNT;
        axis++
      ) {
        flow_h->face_sample_sum_mps2[axis] +=
          raw_sample->accel_mps2[axis];
      }
      flow_h->face_sample_count++;
      if (flow_h->face_sample_count < APP_FACE_SAMPLE_COUNT) {
        return APP_CALIBRATION_EVENT_NONE;
      }

      float mean_mps2[APP_CALIBRATION_AXIS_COUNT];
      for (
        uint8_t axis = 0u;
        axis < APP_CALIBRATION_AXIS_COUNT;
        axis++
      ) {
        mean_mps2[axis] =
          flow_h->face_sample_sum_mps2[axis] /
          (float)flow_h->face_sample_count;
      }

      if (!app_calibrationflow_acceptface(
            flow_h,
            flow_h->candidate_face,
            mean_mps2
          )) {
        SYS_LOG_WARN("CAL", "face rejected: invalid span");
        app_calibrationflow_resetface(flow_h, now_ms);
        return APP_CALIBRATION_EVENT_FACE_REJECTED;
      }

      bool stored = App_CalibrationRecord_Commit(
        flow_h->record_store_h,
        &flow_h->working_record
      );
      bool complete = flow_h->face_mask == APP_FACE_ALL;
      if (complete) {
        uint32_t event = app_calibrationflow_entermain(
          flow_h,
          now_ms,
          stored
            ? "calibration complete"
            : "calibration flash fallback"
        );
        return event |
          (stored
            ? APP_CALIBRATION_EVENT_NONE
            : APP_CALIBRATION_EVENT_FACE_COMMIT_FAILED);
      }

      app_calibrationflow_resetface(flow_h, now_ms);
      return stored
        ? APP_CALIBRATION_EVENT_FACE_SAVED
        : APP_CALIBRATION_EVENT_FACE_COMMIT_FAILED;

    default:
      return APP_CALIBRATION_EVENT_NONE;
  }
}

/**
 * @brief 初始化标定流程。
 * @param flow_h 待初始化流程，不得为 NULL。
 * @param record_store_h 已初始化校准记录存储，不得为 NULL。
 * @retval true 流程进入 INITIAL_POSE_CHECK。
 * @retval false 任一指针为空。
 * @pre 记录存储已成功初始化，调用发生在调度器启动前。
 * @post working record 复制最近活动记录，全部采样和计时状态清零。
 * @note 不访问 Flash、不分配内存。
 */
bool App_CalibrationFlow_Init(
  app_calibration_flow_t *flow_h,
  app_calibration_record_store_t *record_store_h
) {
  if (flow_h == NULL || record_store_h == NULL) {
    return false;
  }

  memset(flow_h, 0, sizeof(*flow_h));
  flow_h->record_store_h = record_store_h;
  flow_h->working_record = *App_CalibrationRecord_GetActive(
    record_store_h
  );
  flow_h->face_mask = flow_h->working_record.face_mask;
  flow_h->completed_axis_mask =
    flow_h->working_record.completed_axis_mask;
  flow_h->state = APP_FAST_CHANNEL_STATE_INITIAL_POSE_CHECK;
  flow_h->initialized = true;
  return true;
}

/**
 * @brief 推进一次初始姿态、陀螺零偏或六面标定状态机。
 * @param flow_h 已初始化流程，不得为 NULL。
 * @param raw_valid 原始样本是否有有效帧。
 * @param raw_sample 原始样本；raw_valid 为 true 时不得为 NULL。
 * @param calibrated_valid 校正样本是否有有效帧。
 * @param calibrated_sample 校正样本；calibrated_valid 为 true 时不得为 NULL。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @return 本次产生的事件位组合，可为 NONE。
 * @pre 只由单一 IMU 任务调用，且流程已成功初始化。
 * @post 状态、采样累计、面掩码和工作记录按样本推进；达到条件时可能提交 Flash。
 * @note 可能执行阻塞 Flash 操作；不分配内存，不使用 GPIO 或控制 EKF。
 */
uint32_t App_CalibrationFlow_Process(
  app_calibration_flow_t *flow_h,
  bool raw_valid,
  const imu_sample_t *raw_sample,
  bool calibrated_valid,
  const imu_sample_t *calibrated_sample,
  uint32_t now_ms
) {
  assert(flow_h);
  if (!flow_h->initialized) {
    return APP_CALIBRATION_EVENT_NONE;
  }

  if (!flow_h->started) {
    flow_h->started = true;
    flow_h->state_started_ms = now_ms;
  }

  if (!raw_valid) {
    if (
      flow_h->state != APP_FAST_CHANNEL_STATE_MAIN &&
      Sys_Time_HasElapsedMs(
        now_ms,
        flow_h->state_started_ms,
        APP_CALIBRATION_FACE_TIMEOUT_MS
      )
    ) {
      return app_calibrationflow_entermain(
        flow_h,
        now_ms,
        "IMU unavailable"
      );
    }
    return APP_CALIBRATION_EVENT_NONE;
  }

  assert(raw_sample);
  switch (flow_h->state) {
    case APP_FAST_CHANNEL_STATE_INITIAL_POSE_CHECK:
      return app_calibrationflow_processinitial(
        flow_h,
        raw_sample,
        now_ms
      );
    case APP_FAST_CHANNEL_STATE_GYRO_CALIBRATION:
      return app_calibrationflow_processgyro(flow_h, raw_sample, now_ms);
    case APP_FAST_CHANNEL_STATE_WAIT_FACE:
    case APP_FAST_CHANNEL_STATE_STABILIZE_FACE:
    case APP_FAST_CHANNEL_STATE_SAMPLE_FACE:
      if (Sys_Time_HasReachedMs(now_ms, flow_h->face_deadline_ms)) {
        return app_calibrationflow_entermain(
          flow_h,
          now_ms,
          "face calibration timeout"
        );
      }
      if (!calibrated_valid) {
        return APP_CALIBRATION_EVENT_NONE;
      }
      assert(calibrated_sample);
      return app_calibrationflow_processface(
        flow_h,
        raw_sample,
        calibrated_sample,
        now_ms
      );
    case APP_FAST_CHANNEL_STATE_MAIN:
    case APP_FAST_CHANNEL_STATE_ERROR:
    case APP_FAST_CHANNEL_STATE_UNINITIALIZED:
    default:
      return APP_CALIBRATION_EVENT_NONE;
  }
}

/**
 * @brief 取得当前标定状态。
 * @param flow_h 流程对象；可为 NULL。
 * @return 当前状态；NULL 时返回 UNINITIALIZED。
 * @pre 无。
 * @post 不修改流程。
 * @note 结果供快照和 LED 周期选择使用。
 */
app_fast_channel_state_t App_CalibrationFlow_GetState(
  const app_calibration_flow_t *flow_h
) {
  return flow_h == NULL
    ? APP_FAST_CHANNEL_STATE_UNINITIALIZED
    : flow_h->state;
}

/**
 * @brief 取得已接受安装面位掩码。
 * @param flow_h 流程对象；可为 NULL。
 * @return 已接受面位掩码；NULL 时返回 0。
 * @pre 无。
 * @post 不修改流程。
 * @note 位定义来自 app_calibration.h。
 */
uint32_t App_CalibrationFlow_GetFaceMask(
  const app_calibration_flow_t *flow_h
) {
  return flow_h == NULL ? 0u : flow_h->face_mask;
}

/**
 * @brief 取得已完成正负样本对的轴位掩码。
 * @param flow_h 流程对象；可为 NULL。
 * @return 已完成轴位掩码；NULL 时返回 0。
 * @pre 无。
 * @post 不修改流程。
 * @note 位 0..2 分别对应 X/Y/Z。
 */
uint32_t App_CalibrationFlow_GetCompletedAxisMask(
  const app_calibration_flow_t *flow_h
) {
  return flow_h == NULL ? 0u : flow_h->completed_axis_mask;
}
