/**
 * @file app_car_control.c
 * @brief F407 四轮小车 100 Hz 控制步骤。
 * @ingroup app
 */

#include "app_car_internal.h"
#include "app_car_config.h"
#include "system_log.h"
#include "system_math.h"
#include "system_time_window.h"
#include <math.h>

/**
 * @brief 运行一次输入映射、底盘解算和四轮闭环。
 * @param state_h App 状态，不得为 NULL。
 * @param now_ms 当前时间，单位 ms。
 * @post 心跳已更新，安全判断、底盘更新和电机闭环已推进。
 * @note 只允许控制任务调用。
 */
void app_car_process_control(app_car_state_t *state_h, uint32_t now_ms) {
  const app_car_dependencies_t *dependencies = state_h->dependencies_h;
  app_car_set_heartbeat(state_h, now_ms);

  if (state_h->faulted) {
    app_car_estop(state_h);
    return;
  }

  if (!state_h->imu_ready || state_h->boot_state != APP_CAR_BOOT_READY) {
    app_car_stop_motion(state_h);
    return;
  }

  joycon_input_t joycon_input = {0};
  if (!Dev_JoyCon_Base_GetInput(dependencies->input_h, &joycon_input) ||
      Sys_Time_IsExpiredMs(now_ms, joycon_input.time_ms, APP_CAR_COMMAND_TIMEOUT_MS)) {
    app_car_stop_motion(state_h);
    return;
  }

  imu_sample_t imu_sample = {0};
  if (!Dev_IMU_Base_GetSample(dependencies->imu_h, &imu_sample) ||
      !app_car_imu_yaw_is_fresh(&imu_sample, now_ms)) {
    app_car_stop_motion(state_h);
    return;
  }

  float left_x = SYS_CLAMP(joycon_input.left_x, -1.0f, 1.0f);
  float left_y = SYS_CLAMP(joycon_input.left_y, -1.0f, 1.0f);
  float right_x = SYS_CLAMP(joycon_input.right_x, -1.0f, 1.0f);
  float input_magnitude = sqrtf(left_x * left_x + left_y * left_y);

  if (input_magnitude <= APP_CAR_COMMAND_DEADBAND_NORM &&
      SYS_ABS(right_x) <= APP_CAR_COMMAND_DEADBAND_NORM) {
    app_car_stop_motion(state_h);
    return;
  }

  chassis_twist_t twist = {
      .vx_body_mps = left_x * APP_CAR_MAX_LINEAR_MPS,
      .vy_body_mps = left_y * APP_CAR_MAX_LINEAR_MPS,
      .wz_radps = right_x * APP_CAR_MAX_ANGULAR_RADPS,
  };
  float linear_speed_mps = hypotf(twist.vx_body_mps, twist.vy_body_mps);

  if (linear_speed_mps > APP_CAR_MAX_LINEAR_MPS) {
    float scale = APP_CAR_MAX_LINEAR_MPS / linear_speed_mps;
    twist.vx_body_mps *= scale;
    twist.vy_body_mps *= scale;
  }

  if (!state_h->motors_enabled) {
    app_car_enable_motors(state_h);
    if (state_h->faulted) {
      app_car_estop(state_h);
      return;
    }
  }

  App_Car_Motion_SetTwist(&state_h->motion, &twist);
  app_car_motion_status_t motion_status = App_Car_Motion_Update(&state_h->motion, now_ms);
  if (motion_status == APP_CAR_MOTION_MOTORS_DISABLED) {
    state_h->motors_enabled = false;
    app_car_clear_command(state_h);
    return;
  }
  if (motion_status == APP_CAR_MOTION_CONTROLLER_ERROR) {
    app_car_set_fault(state_h, "motor controller error");
    app_car_estop(state_h);
    return;
  }

  app_car_set_command(state_h, &joycon_input, &twist, now_ms);
}
