/**
 * @file app_car_safety.c
 * @brief F407 四轮小车故障、停止、急停和电机安全控制。
 * @ingroup app
 */

#include "app_car_internal.h"
#include "app_car_config.h"
#include "system_log.h"

/**
 * @brief 首次故障时锁存 faulted 并记录错误。
 * @param state_h App 状态，不得为 NULL。
 * @param reason 故障原因文本，不得为 NULL。
 * @post 首次故障时置位并写日志。
 * @note 不加锁，只锁存故障。
 */
void app_car_set_fault(app_car_state_t *state_h, const char *reason) {
  if (!state_h->faulted) {
    state_h->faulted = true;
    SYS_LOG_ERROR("CAR", "%s", reason);
  }
}

/**
 * @brief 常规制动底盘并清除运动命令。
 * @param state_h App 状态，不得为 NULL。
 * @post 已使能或急停锁存时请求底盘停车，随后清除使能标记和命令。
 * @note 只允许控制任务调用。
 */
void app_car_stop_motion(app_car_state_t *state_h) {
  if (state_h->motors_enabled || state_h->estop_latched) {
    App_Car_Motion_Stop(&state_h->motion);
  }

  state_h->motors_enabled = false;
  state_h->estop_latched = false;
  app_car_clear_command(state_h);
}

/**
 * @brief 执行并锁存一次急停。
 * @param state_h App 状态，不得为 NULL。
 * @post 首次调用请求底盘急停，随后清除使能标记和命令。
 * @note 只允许控制或监控任务调用。
 */
void app_car_estop(app_car_state_t *state_h) {
  if (!state_h->estop_latched) {
    App_Car_Motion_EStop(&state_h->motion);
    state_h->estop_latched = true;
  }

  state_h->motors_enabled = false;
  app_car_clear_command(state_h);
}

/**
 * @brief 检查四个电机是否全部使能。
 * @param state_h App 状态，不得为 NULL。
 * @retval true 四个电机均为 ENABLE。
 * @retval false 任一电机状态不是 ENABLE。
 * @note 只读取电机状态。
 */
bool app_car_motors_are_enabled(const app_car_state_t *state_h) {
  return App_Car_Motion_MotorsAreEnabled(&state_h->motion);
}

/**
 * @brief 使能四个轮电机并回读状态。
 * @param state_h App 状态，不得为 NULL。
 * @post 四轮已请求使能，motors_enabled 反映回读结果。
 * @post 回读失败时锁存电机使能故障。
 * @note 只允许控制任务调用。
 */
void app_car_enable_motors(app_car_state_t *state_h) {
  state_h->motors_enabled = App_Car_Motion_EnableMotors(&state_h->motion);
  state_h->estop_latched = false;

  if (!state_h->motors_enabled) {
    app_car_set_fault(state_h, "motor enable failed");
  }
}

/**
 * @brief 推进控制心跳和故障安全处理。
 * @param state_h App 状态，不得为 NULL。
 * @param now_ms 当前时间，单位 ms。
 * @post 心跳超时或已有故障时执行急停。
 * @note 只允许监控任务调用。
 */
void app_car_process_safety(app_car_state_t *state_h, uint32_t now_ms) {
  if (state_h->control_heartbeat_valid &&
      (uint32_t)(now_ms - state_h->control_heartbeat_ms) > APP_CAR_HEARTBEAT_TIMEOUT_MS) {
    app_car_set_fault(state_h, "control heartbeat timeout");
    app_car_estop(state_h);
  } else if (state_h->faulted) {
    app_car_estop(state_h);
  }
}
