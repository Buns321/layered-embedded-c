/**
 * @file app_fast_channel_telemetry.c
 * @brief 快速通道任务一 32 通道遥测映射实现。
 * @ingroup app
 */

#include "app_fast_channel_telemetry.h"
#include "app_imu_session.h"
#include "app_fast_channel_config.h"
#include "system_log.h"
#include "system_time_window.h"
#include <assert.h>
#include <stddef.h>

#define APP_TELEMETRY_CHANNEL_RAW_ACCEL_X 0u
#define APP_TELEMETRY_CHANNEL_CAL_ACCEL_X 3u
#define APP_TELEMETRY_CHANNEL_RAW_GYRO_X 6u
#define APP_TELEMETRY_CHANNEL_CAL_GYRO_X 9u
#define APP_TELEMETRY_CHANNEL_DIRECT_ROLL 12u
#define APP_TELEMETRY_CHANNEL_DIRECT_PITCH 13u
#define APP_TELEMETRY_CHANNEL_INTEGRATED_ROLL 14u
#define APP_TELEMETRY_CHANNEL_INTEGRATED_PITCH 15u
#define APP_TELEMETRY_CHANNEL_INTEGRATED_YAW 16u
#define APP_TELEMETRY_CHANNEL_EKF_ROLL 17u
#define APP_TELEMETRY_CHANNEL_EKF_PITCH 18u
#define APP_TELEMETRY_CHANNEL_EKF_YAW 19u
#define APP_TELEMETRY_CHANNEL_QUATERNION 20u
#define APP_TELEMETRY_CHANNEL_BIAS_X 24u
#define APP_TELEMETRY_CHANNEL_DT 27u
#define APP_TELEMETRY_CHANNEL_CAL_STATE 28u
#define APP_TELEMETRY_CHANNEL_FACE_MASK 29u
#define APP_TELEMETRY_CHANNEL_EKF_STATUS 30u
#define APP_TELEMETRY_CHANNEL_COMMAND_STATUS 31u
#define APP_FAST_CHANNEL_DEG_TO_RAD 0.01745329251994329577f

_Static_assert(
  APP_TELEMETRY_CHANNEL_COUNT <= TELEMETRY_CHANNEL_CAPACITY,
  "JustFloat channel count exceeds telemetry capacity"
);

/**
 * @brief 设置一个遥测通道有效位。
 * @param sample 待修改遥测样本，不得为 NULL。
 * @param channel 通道下标，必须小于遥测容量。
 * @param valid true 置位，false 清零。
 * @pre sample 可写且 channel 在容量内。
 * @post 仅修改指定通道对应的有效位。
 * @note 纯本地操作，不访问硬件、不分配内存。
 */
static void app_fastchanneltelemetry_setchannelvalid(
  telemetry_sample_t *sample,
  uint32_t channel,
  bool valid
) {
  assert(sample && channel < TELEMETRY_CHANNEL_CAPACITY);
  uint32_t mask = (uint32_t)1u << channel;
  if (valid) {
    sample->valid_mask |= mask;
  } else {
    sample->valid_mask &= ~mask;
  }
}

/**
 * @brief 初始化遥测模块状态。
 * @param telemetry_h 待初始化模块状态，不得为 NULL。
 * @post 日志节流和链路溢出观察计数清零。
 * @note 不访问硬件、不分配内存。
 */
void App_FastChannelTelemetry_Init(
  app_fast_channel_telemetry_t *telemetry_h
) {
  assert(telemetry_h);
  *telemetry_h = (app_fast_channel_telemetry_t){0};
}

/**
 * @brief 组装并发送一帧 32 通道 JustFloat，同时输出低频状态日志。
 * @param telemetry_h 已初始化模块状态，不得为 NULL。
 * @param telemetry_base_h 已初始化遥测发送后端，不得为 NULL。
 * @param snapshot 当前 IMU 会话快照，不得为 NULL。
 * @param link_status 当前 UART 链路状态，不得为 NULL。
 * @param command_status 最近一次 App 命令结果。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @pre 只由 VofaTask 调用。
 * @post 已发起一次非阻塞整帧发送；忙时由后端整帧丢弃。
 * @note 不阻塞等待 DMA、不分配内存。
 */
void App_FastChannelTelemetry_Process(
  app_fast_channel_telemetry_t *telemetry_h,
  telemetry_base_t *telemetry_base_h,
  const app_fast_channel_snapshot_t *snapshot,
  const uart_link_status_t *link_status,
  app_fast_channel_command_status_t command_status,
  uint32_t now_ms
) {
  assert(telemetry_h && telemetry_base_h && snapshot && link_status);

  if (
    link_status->line_overflow_count !=
    telemetry_h->observed_line_overflow_count
  ) {
    telemetry_h->observed_line_overflow_count =
      link_status->line_overflow_count;
    command_status = APP_FAST_CHANNEL_COMMAND_OVERFLOW;
  }

  telemetry_sample_t telemetry = {
    .valid_mask = TELEMETRY_CHANNEL_MASK,
    .time_ms = now_ms,
  };

  for (uint8_t axis = 0u; axis < 3u; axis++) {
    telemetry.value[APP_TELEMETRY_CHANNEL_RAW_ACCEL_X + axis] =
      snapshot->raw_sample.accel_mps2[axis];
    telemetry.value[APP_TELEMETRY_CHANNEL_CAL_ACCEL_X + axis] =
      snapshot->calibrated_sample.accel_mps2[axis];
    telemetry.value[APP_TELEMETRY_CHANNEL_RAW_GYRO_X + axis] =
      snapshot->raw_sample.gyro_dps[axis] *
      APP_FAST_CHANNEL_DEG_TO_RAD;
    telemetry.value[APP_TELEMETRY_CHANNEL_CAL_GYRO_X + axis] =
      snapshot->calibrated_sample.gyro_dps[axis] *
      APP_FAST_CHANNEL_DEG_TO_RAD;

    app_fastchanneltelemetry_setchannelvalid(
      &telemetry,
      APP_TELEMETRY_CHANNEL_RAW_ACCEL_X + axis,
      snapshot->raw_valid
    );
    app_fastchanneltelemetry_setchannelvalid(
      &telemetry,
      APP_TELEMETRY_CHANNEL_RAW_GYRO_X + axis,
      snapshot->raw_valid
    );
    app_fastchanneltelemetry_setchannelvalid(
      &telemetry,
      APP_TELEMETRY_CHANNEL_CAL_ACCEL_X + axis,
      snapshot->calibrated_valid
    );
    app_fastchanneltelemetry_setchannelvalid(
      &telemetry,
      APP_TELEMETRY_CHANNEL_CAL_GYRO_X + axis,
      snapshot->calibrated_valid
    );
    telemetry.value[APP_TELEMETRY_CHANNEL_BIAS_X + axis] =
      snapshot->ekf_state.gyro_bias_radps[axis];
    app_fastchanneltelemetry_setchannelvalid(
      &telemetry,
      APP_TELEMETRY_CHANNEL_BIAS_X + axis,
      snapshot->ekf_state.valid && !snapshot->ekf_state.placeholder
    );
  }

  telemetry.value[APP_TELEMETRY_CHANNEL_DIRECT_ROLL] =
    snapshot->direct_roll_rad;
  telemetry.value[APP_TELEMETRY_CHANNEL_DIRECT_PITCH] =
    snapshot->direct_pitch_rad;
  telemetry.value[APP_TELEMETRY_CHANNEL_INTEGRATED_ROLL] =
    snapshot->integrated_roll_rad;
  telemetry.value[APP_TELEMETRY_CHANNEL_INTEGRATED_PITCH] =
    snapshot->integrated_pitch_rad;
  telemetry.value[APP_TELEMETRY_CHANNEL_INTEGRATED_YAW] =
    snapshot->integrated_yaw_rad;
  telemetry.value[APP_TELEMETRY_CHANNEL_EKF_ROLL] =
    snapshot->ekf_state.roll_rad;
  telemetry.value[APP_TELEMETRY_CHANNEL_EKF_PITCH] =
    snapshot->ekf_state.pitch_rad;
  telemetry.value[APP_TELEMETRY_CHANNEL_EKF_YAW] =
    snapshot->ekf_state.yaw_rad;

  for (uint8_t index = 0u; index < 4u; index++) {
    telemetry.value[APP_TELEMETRY_CHANNEL_QUATERNION + index] =
      snapshot->ekf_state.quaternion[index];
  }

  for (
    uint32_t channel = APP_TELEMETRY_CHANNEL_DIRECT_ROLL;
    channel <= APP_TELEMETRY_CHANNEL_INTEGRATED_YAW;
    channel++
  ) {
    app_fastchanneltelemetry_setchannelvalid(
      &telemetry,
      channel,
      snapshot->calibrated_valid
    );
  }
  for (
    uint32_t channel = APP_TELEMETRY_CHANNEL_EKF_ROLL;
    channel <= APP_TELEMETRY_CHANNEL_QUATERNION + 3u;
    channel++
  ) {
    app_fastchanneltelemetry_setchannelvalid(
      &telemetry,
      channel,
      snapshot->ekf_state.valid && !snapshot->ekf_state.placeholder
    );
  }

  telemetry.value[APP_TELEMETRY_CHANNEL_DT] = snapshot->dt_s;
  telemetry.value[APP_TELEMETRY_CHANNEL_CAL_STATE] =
    (float)snapshot->calibration_state;
  telemetry.value[APP_TELEMETRY_CHANNEL_FACE_MASK] =
    (float)snapshot->face_mask;
  telemetry.value[APP_TELEMETRY_CHANNEL_EKF_STATUS] =
    snapshot->ekf_state.placeholder
      ? 100.0f
      : (snapshot->ekf_state.valid ? 1.0f : 0.0f);
  telemetry.value[APP_TELEMETRY_CHANNEL_COMMAND_STATUS] =
    (float)command_status;

  (void)Dev_Telemetry_Base_Send(telemetry_base_h, &telemetry);

  if (
    telemetry_h->last_log_ms == 0u ||
    Sys_Time_HasElapsedMs(now_ms, telemetry_h->last_log_ms, 1000u)
  ) {
    telemetry_h->last_log_ms = now_ms;
    telemetry_status_t transport_status = {0};
    Dev_Telemetry_Base_GetStatus(telemetry_base_h, &transport_status);
    SYS_LOG_INFO(
      "RUN",
      "state=%u face=%u flash=%u cmd=%u drop=%lu us=%lu jitter=%lu stack=%lu",
      (unsigned int)snapshot->calibration_state,
      (unsigned int)snapshot->face_mask,
      snapshot->flash_record_valid ? 1u : 0u,
      (unsigned int)command_status,
      (unsigned long)transport_status.dropped_frames,
      (unsigned long)snapshot->metrics.execution_us,
      (unsigned long)snapshot->metrics.period_jitter_ms,
      (unsigned long)snapshot->metrics.stack_free_bytes
    );
  }
}
