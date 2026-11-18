/**
 * @file app_car_command.c
 * @brief F407 四轮小车运动命令快照。
 * @ingroup app
 */

#include "app_car_internal.h"
#include "system_snapshot.h"

/**
 * @brief 在临界区内清除有效运动命令。
 * @param state_h App 状态，不得为 NULL。
 * @post command 整体清零，valid 为 false。
 * @note 使用短临界区保护跨任务快照。
 */
void app_car_clear_command(app_car_state_t *state_h) {
  const app_car_command_t cleared = {0};
  SYS_SNAPSHOT_WRITE(&state_h->command, &cleared);
}

/**
 * @brief 在临界区内发布运动命令和输入快照。
 * @param state_h App 状态，不得为 NULL。
 * @param input 输入快照，不得为 NULL。
 * @param twist 车体系速度命令；不可为 NULL。
 * @param now_ms 命令时间戳，单位 ms。
 * @post command 被整体覆盖并置 valid。
 * @note 使用短临界区保证快照一致性。
 */
void app_car_set_command(app_car_state_t *state_h, const joycon_input_t *input,
                         const chassis_twist_t *twist, uint32_t now_ms) {
  const app_car_command_t next_command = {
      .twist = *twist,
      .joycon_left_x = input->left_x,
      .joycon_left_y = input->left_y,
      .joycon_right_x = input->right_x,
      .time_ms = now_ms,
      .valid = true,
  };
  SYS_SNAPSHOT_WRITE(&state_h->command, &next_command);
}

/**
 * @brief 在临界区内复制当前运动命令。
 * @param state_h App 状态，不得为 NULL。
 * @return 当前命令副本，内容可能为 invalid。
 * @post 不修改 App 状态。
 * @note 使用短临界区保证整条命令一致。
 */
app_car_command_t app_car_get_command(app_car_state_t *state_h) {
  app_car_command_t command;
  SYS_SNAPSHOT_READ(&command, &state_h->command);
  return command;
}

/**
 * @brief 在临界区内发布控制任务心跳快照。
 * @param state_h App 状态，不得为 NULL。
 * @param now_ms 当前控制任务时间，单位 ms。
 * @post control_heartbeat 已原子性更新为 valid=true、time_ms=now_ms。
 * @note 使用 System 临界区保证跨任务快照一致。
 */
void app_car_set_heartbeat(app_car_state_t *state_h, uint32_t now_ms) {
  const app_car_heartbeat_t heartbeat = {
      .time_ms = now_ms,
      .valid = true,
  };
  SYS_SNAPSHOT_WRITE(&state_h->control_heartbeat, &heartbeat);
}

/**
 * @brief 在临界区内读取控制任务心跳快照。
 * @param state_h App 状态，不得为 NULL。
 * @return 控制任务心跳快照副本。
 * @post 不修改 App 状态。
 * @note 使用 System 临界区保证 valid 与 time_ms 成对一致。
 */
app_car_heartbeat_t app_car_get_heartbeat(const app_car_state_t *state_h) {
  app_car_heartbeat_t heartbeat;
  SYS_SNAPSHOT_READ(&heartbeat, &state_h->control_heartbeat);
  return heartbeat;
}
