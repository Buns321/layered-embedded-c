/**
 * @file app_car_command.c
 * @brief F407 四轮小车运动命令快照。
 * @ingroup app
 */

#include "app_car_internal.h"
#include "system_critical.h"

/**
 * @brief 在临界区内清除有效运动命令。
 * @param state_h App 状态，不得为 NULL。
 * @post command.valid 置 false，线速度、方向角和角速度清零。
 * @note 使用短临界区保护跨任务快照。
 */
void app_car_clear_command(app_car_state_t *state_h) {
  Sys_Critical_Enter();
  state_h->command.valid = false;
  state_h->command.twist = (chassis_twist_t){0};
  Sys_Critical_Exit();
}

/**
 * @brief 在临界区内发布运动命令和输入快照。
 * @param state_h App 状态，不得为 NULL。
 * @param input JoyCon 输入快照，不得为 NULL。
 * @param twist 车体系速度命令；不可为 NULL。
 * @param now_ms 命令时间戳，单位 ms。
 * @post command 被整体覆盖并置 valid。
 * @note 使用短临界区保证快照一致性。
 */
void app_car_set_command(app_car_state_t *state_h, const joycon_input_t *input,
                         const chassis_twist_t *twist, uint32_t now_ms) {
  Sys_Critical_Enter();
  state_h->command = (app_car_command_t){
      .twist = *twist,
      .joycon_left_x = input->left_x,
      .joycon_left_y = input->left_y,
      .joycon_right_x = input->right_x,
      .time_ms = now_ms,
      .valid = true,
  };
  Sys_Critical_Exit();
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

  Sys_Critical_Enter();
  command = state_h->command;
  Sys_Critical_Exit();

  return command;
}
