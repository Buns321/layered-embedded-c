/**
 * @file app_car_telemetry.c
 * @brief F407 四轮小车遥测样本和状态日志。
 * @ingroup app
 */

#include "app_car_internal.h"
#include "app_car_config.h"
#include "system_log.h"

/**
 * @brief 根据有效电机数量构造遥测有效位掩码。
 * @param motor_count 有效电机数量，范围 0..APP_CAR_MOTOR_CAPACITY。
 * @return 包含公共通道和有效轮速通道的掩码；数量越界时返回 0。
 * @post 未使用电机对应的目标/实测轮速位被清除。
 * @note 纯函数，不修改 App 状态。
 */
uint32_t app_car_build_telemetry_mask(uint8_t motor_count) {
  if (motor_count > APP_CAR_MOTOR_CAPACITY) {
    return 0U;
  }

  uint32_t valid_mask = TELEMETRY_CHANNEL_MASK;
  for (uint8_t wheel = motor_count; wheel < APP_CAR_MOTOR_CAPACITY; wheel++) {
    valid_mask &= ~(uint32_t)(1UL << (APP_CAR_TELEMETRY_TARGET_A + wheel));
    valid_mask &= ~(uint32_t)(1UL << (APP_CAR_TELEMETRY_CURRENT_A + wheel));
  }
  return valid_mask;
}

/**
 * @brief 按周期组装并提交 16 通道车辆遥测。
 * @param state_h App 状态，不得为 NULL。
 * @param now_ms 样本时间，单位 ms。
 * @post 到达发布周期时更新 last_telemetry_ms 并提交样本。
 * @note 发送忙时整帧丢弃，不等待 DMA。
 */
void app_car_process_telemetry(app_car_state_t *state_h, uint32_t now_ms) {
  if (state_h->last_telemetry_ms != 0U &&
      (uint32_t)(now_ms - state_h->last_telemetry_ms) < APP_CAR_TELEMETRY_PERIOD_MS) {
    return;
  }

  state_h->last_telemetry_ms = now_ms;
  const app_car_dependencies_t *dependencies = state_h->dependencies_h;
  telemetry_sample_t sample = {
      .valid_mask = app_car_build_telemetry_mask(state_h->motion.motor_count),
      .time_ms = now_ms,
  };
  imu_sample_t imu_sample = {0};
  joycon_input_t joycon_input = {0};
  app_car_command_t command = app_car_get_command(state_h);

  for (uint8_t wheel = 0U; wheel < state_h->motion.motor_count; wheel++) {
    sample.value[APP_CAR_TELEMETRY_TARGET_A + wheel] =
        App_Car_Motion_ReadTargetSpeed(&state_h->motion, wheel);
    sample.value[APP_CAR_TELEMETRY_CURRENT_A + wheel] =
        App_Car_Motion_ReadCurrentSpeed(&state_h->motion, wheel);
  }

  sample.value[APP_CAR_TELEMETRY_VX_BODY] = command.twist.vx_body_mps;
  sample.value[APP_CAR_TELEMETRY_VY_BODY] = command.twist.vy_body_mps;
  sample.value[APP_CAR_TELEMETRY_WZ] = command.twist.wz_radps;

  if (Dev_IMU_Base_GetSample(dependencies->imu_h, &imu_sample)) {
    if ((imu_sample.valid_mask & IMU_CHANNEL_YAW) != 0U) {
      sample.value[APP_CAR_TELEMETRY_YAW] = imu_sample.yaw_deg;
    } else {
      sample.valid_mask &= ~(uint32_t)(1UL << APP_CAR_TELEMETRY_YAW);
    }

    if ((imu_sample.valid_mask & IMU_CHANNEL_GYRO_Z) != 0U) {
      sample.value[APP_CAR_TELEMETRY_GYRO_Z] = imu_sample.gyro_dps[2];
    } else {
      sample.valid_mask &= ~(uint32_t)(1UL << APP_CAR_TELEMETRY_GYRO_Z);
    }
  } else {
    sample.valid_mask &=
        ~(uint32_t)((1UL << APP_CAR_TELEMETRY_YAW) | (1UL << APP_CAR_TELEMETRY_GYRO_Z));
  }

  if (Dev_JoyCon_Base_GetInput(dependencies->input_h, &joycon_input)) {
    sample.value[APP_CAR_TELEMETRY_JOYCON_LEFT_X] = joycon_input.left_x;
    sample.value[APP_CAR_TELEMETRY_JOYCON_LEFT_Y] = joycon_input.left_y;
    sample.value[APP_CAR_TELEMETRY_JOYCON_RIGHT_X] = joycon_input.right_x;
  } else {
    sample.valid_mask &= ~(uint32_t)((1UL << APP_CAR_TELEMETRY_JOYCON_LEFT_X) |
                                     (1UL << APP_CAR_TELEMETRY_JOYCON_LEFT_Y) |
                                     (1UL << APP_CAR_TELEMETRY_JOYCON_RIGHT_X));
  }

  (void)Dev_Telemetry_Base_Send(dependencies->telemetry_h, &sample);
}

/**
 * @brief 按周期输出一条车辆运行状态日志。
 * @param state_h App 状态，不得为 NULL。
 * @param now_ms 当前时间，单位 ms。
 * @post 到达日志周期时更新 last_log_ms 并输出状态。
 * @note 不修改车辆状态。
 */
void app_car_process_log(app_car_state_t *state_h, uint32_t now_ms) {
  if (state_h->last_log_ms != 0U &&
      (uint32_t)(now_ms - state_h->last_log_ms) < APP_CAR_LOG_PERIOD_MS) {
    return;
  }

  state_h->last_log_ms = now_ms;
  const app_car_dependencies_t *dependencies = state_h->dependencies_h;
  imu_status_t imu_status = {0};
  joycon_status_t joycon_status = {0};
  telemetry_status_t telemetry_status = {0};

  Dev_IMU_Base_GetStatus(dependencies->imu_h, &imu_status);
  Dev_JoyCon_Base_GetStatus(dependencies->input_h, &joycon_status);
  Dev_Telemetry_Base_GetStatus(dependencies->telemetry_h, &telemetry_status);

  SYS_LOG_INFO(
      "CAR", "state=%u imu_err=%lu parser_err=%lu rx_ovf=%lu jf_drop=%lu hb_age=%lu",
      (unsigned int)state_h->boot_state, (unsigned long)imu_status.error_count,
      (unsigned long)joycon_status.invalid_frames, (unsigned long)joycon_status.rx_overflow_count,
      (unsigned long)telemetry_status.dropped_frames,
      (unsigned long)(state_h->control_heartbeat_valid ? now_ms - state_h->control_heartbeat_ms
                                                       : 0U));
}
