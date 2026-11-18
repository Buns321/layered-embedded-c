/**
 * @file app_car_boot.c
 * @brief F407 四轮小车启动和输入状态机。
 * @ingroup app
 */

#include "app_car_internal.h"
#include "app_car_config.h"
#include "system_log.h"
#include "system_math.h"
#include "system_time_window.h"
#include <assert.h>

/**
 * @brief 判断时间戳是否晚于参考点。
 * @param timestamp_ms 待比较时间戳，单位 ms。
 * @param reference_ms 参考时间戳，单位 ms。
 * @retval true 有符号差大于零。
 * @retval false 时间戳等于或早于参考点。
 * @note 要求间隔小于 2^31 ms。
 */
static bool app_car_timestamp_is_after(uint32_t timestamp_ms, uint32_t reference_ms) {
  return (int32_t)(timestamp_ms - reference_ms) > 0;
}

/**
 * @brief 切换启动状态并重置静止检测计时。
 * @param state_h App 状态，不得为 NULL。
 * @param state 新状态。
 * @param now_ms 状态时间戳，单位 ms。
 * @post 状态和时间更新，静止检测复位。
 * @note 只允许输入任务调用。
 */
void app_car_enter_state(app_car_state_t *state_h, app_car_boot_state_t state, uint32_t now_ms) {
  state_h->boot_state = state;
  state_h->state_started_ms = now_ms;
  state_h->still_since_ms = 0U;
  state_h->still_tracking = false;
}

/**
 * @brief 检查 yaw 与 gyro-Z 有效位和 yaw 新鲜度。
 * @param sample IMU 快照，不得为 NULL。
 * @param now_ms 当前时间，单位 ms。
 * @retval true 通道有效且 yaw 未超时。
 * @retval false 通道缺失或 yaw 过期。
 * @note 时间差按无符号回绕处理。
 */
bool app_car_imu_yaw_is_fresh(const imu_sample_t *sample, uint32_t now_ms) {
  assert(sample);

  if ((sample->valid_mask & IMU_CHANNEL_YAW) == 0U ||
      (sample->valid_mask & IMU_CHANNEL_GYRO_Z) == 0U) {
    return false;
  }

  uint32_t yaw_age_ms = now_ms - sample->channel_update_ms[IMU_CHANNEL_INDEX_YAW];

  return yaw_age_ms <= APP_CAR_IMU_TIMEOUT_MS;
}

/**
 * @brief 推进 IMU 速率配置、静止确认、置零和验证状态机。
 * @param state_h App 状态，不得为 NULL。
 * @param now_ms 当前时间，单位 ms。
 * @post 启动状态按 IMU 状态推进，失败时设置故障。
 * @note 只允许输入任务调用。
 */
static void app_car_update_boot_state(app_car_state_t *state_h, uint32_t now_ms) {
  imu_status_t imu_status = {0};
  imu_sample_t imu_sample = {0};
  imu_base_t *imu_h = state_h->dependencies_h->imu_h;

  Dev_IMU_Base_GetStatus(imu_h, &imu_status);

  switch (state_h->boot_state) {
  case APP_CAR_BOOT_RATE_CONFIG:
    if (imu_status.last_request_failed) {
      app_car_set_fault(state_h, "wht101 rate config failed");
      return;
    }

    if (!imu_status.is_busy) {
      state_h->config_done_ms = now_ms;
      app_car_enter_state(state_h, APP_CAR_BOOT_WAIT_STILL, now_ms);
      SYS_LOG_INFO("IMU", "rate configured to 10 Hz");
    } else if (Sys_Time_HasElapsedMs(now_ms, state_h->state_started_ms,
                                     APP_CAR_IMU_CONFIG_TIMEOUT_MS)) {
      app_car_set_fault(state_h, "wht101 rate config timeout");
    }
    break;

  case APP_CAR_BOOT_WAIT_STILL:
    if (!Dev_IMU_Base_GetSample(imu_h, &imu_sample)) {
      break;
    }

    bool has_fresh_channels =
        (imu_sample.valid_mask & IMU_CHANNEL_YAW) != 0U &&
        (imu_sample.valid_mask & IMU_CHANNEL_GYRO_Z) != 0U &&
        app_car_timestamp_is_after(imu_sample.channel_update_ms[IMU_CHANNEL_INDEX_YAW],
                                   state_h->config_done_ms) &&
        app_car_timestamp_is_after(imu_sample.channel_update_ms[IMU_CHANNEL_INDEX_GYRO_Z],
                                   state_h->config_done_ms);
    bool is_still =
        has_fresh_channels && SYS_ABS(imu_sample.gyro_dps[2]) <= APP_CAR_IMU_STILL_GYRO_DPS;

    if (is_still) {
      if (!state_h->still_tracking) {
        state_h->still_since_ms = now_ms;
        state_h->still_tracking = true;
      } else if (Sys_Time_HasElapsedMs(now_ms, state_h->still_since_ms,
                                       APP_CAR_IMU_STILL_CONFIRM_MS)) {
        Dev_IMU_Base_SetZero(imu_h);
        app_car_enter_state(state_h, APP_CAR_BOOT_ZERO_CONFIG, now_ms);
        SYS_LOG_INFO("IMU", "stationary confirmed, writing yaw zero");
      }
    } else {
      state_h->still_tracking = false;
    }

    if (state_h->boot_state == APP_CAR_BOOT_WAIT_STILL &&
        Sys_Time_HasElapsedMs(now_ms, state_h->state_started_ms, APP_CAR_IMU_STILL_TIMEOUT_MS)) {
      app_car_set_fault(state_h, "wht101 is not stationary");
    }
    break;

  case APP_CAR_BOOT_ZERO_CONFIG:
    if (imu_status.last_request_failed) {
      app_car_set_fault(state_h, "wht101 zero write failed");
      return;
    }

    if (!imu_status.is_busy) {
      state_h->zero_done_ms = now_ms;
      app_car_enter_state(state_h, APP_CAR_BOOT_WAIT_ZERO_FRAME, now_ms);
    } else if (Sys_Time_HasElapsedMs(now_ms, state_h->state_started_ms,
                                     APP_CAR_IMU_CONFIG_TIMEOUT_MS)) {
      app_car_set_fault(state_h, "wht101 zero write timeout");
    }
    break;

  case APP_CAR_BOOT_WAIT_ZERO_FRAME:
    if (!Dev_IMU_Base_GetSample(imu_h, &imu_sample)) {
      break;
    }

    if ((imu_sample.valid_mask & IMU_CHANNEL_YAW) != 0U &&
        app_car_timestamp_is_after(imu_sample.channel_update_ms[IMU_CHANNEL_INDEX_YAW],
                                   state_h->zero_done_ms) &&
        SYS_ABS(imu_sample.yaw_deg) <= APP_CAR_IMU_ZERO_RESIDUAL_DEG) {
      Dev_IMU_Base_ZeroYaw(imu_h);
      state_h->imu_ready = true;
      app_car_enter_state(state_h, APP_CAR_BOOT_READY, now_ms);
      SYS_LOG_INFO("IMU", "sensor zero verified, car ready");
    } else if (Sys_Time_HasElapsedMs(now_ms, state_h->state_started_ms,
                                     APP_CAR_IMU_ZERO_TIMEOUT_MS)) {
      app_car_set_fault(state_h, "wht101 zero verify timeout");
    }
    break;

  case APP_CAR_BOOT_READY:
  case APP_CAR_BOOT_FAULT:
    break;
  }
}

/**
 * @brief 推进 IMU、输入设备和启动状态机。
 * @param state_h App 状态，不得为 NULL。
 * @param now_ms 当前时间，单位 ms。
 * @post 输入设备已处理，故障或启动状态已更新。
 * @note 只允许输入任务调用。
 */
void app_car_process_inputs(app_car_state_t *state_h, uint32_t now_ms) {
  const app_car_dependencies_t *dependencies = state_h->dependencies_h;
  Dev_IMU_Base_Process(dependencies->imu_h, now_ms);
  Dev_JoyCon_Base_Process(dependencies->input_h, now_ms);

  if (state_h->faulted) {
    state_h->boot_state = APP_CAR_BOOT_FAULT;
    return;
  }

  if (state_h->boot_state != APP_CAR_BOOT_READY) {
    app_car_update_boot_state(state_h, now_ms);
  }
}
