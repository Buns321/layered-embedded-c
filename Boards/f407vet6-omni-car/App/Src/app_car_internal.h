/**
 * @file app_car_internal.h
 * @brief F407 四轮小车 App 内部状态和模块接口。
 * @ingroup app
 */

#ifndef LAYERED_EMBEDDED_C_APP_CAR_INTERNAL_H
#define LAYERED_EMBEDDED_C_APP_CAR_INTERNAL_H

#include "app_car_motion.h"
#include <stdbool.h>
#include <stdint.h>

/** @cond app_car_internal */

typedef enum app_car_boot_state_e {
  APP_CAR_BOOT_RATE_CONFIG = 0U,
  APP_CAR_BOOT_WAIT_STILL,
  APP_CAR_BOOT_ZERO_CONFIG,
  APP_CAR_BOOT_WAIT_ZERO_FRAME,
  APP_CAR_BOOT_READY,
  APP_CAR_BOOT_FAULT,
} app_car_boot_state_t;

typedef enum app_car_telemetry_channel_e {
  APP_CAR_TELEMETRY_TARGET_A = 0U,
  APP_CAR_TELEMETRY_TARGET_B,
  APP_CAR_TELEMETRY_TARGET_C,
  APP_CAR_TELEMETRY_TARGET_D,
  APP_CAR_TELEMETRY_CURRENT_A,
  APP_CAR_TELEMETRY_CURRENT_B,
  APP_CAR_TELEMETRY_CURRENT_C,
  APP_CAR_TELEMETRY_CURRENT_D,
  APP_CAR_TELEMETRY_VX_BODY,
  APP_CAR_TELEMETRY_VY_BODY,
  APP_CAR_TELEMETRY_WZ,
  APP_CAR_TELEMETRY_YAW,
  APP_CAR_TELEMETRY_GYRO_Z,
  APP_CAR_TELEMETRY_JOYCON_LEFT_X,
  APP_CAR_TELEMETRY_JOYCON_LEFT_Y,
  APP_CAR_TELEMETRY_JOYCON_RIGHT_X,
} app_car_telemetry_channel_t;

typedef struct app_car_command_s {
  chassis_twist_t twist;
  float joycon_left_x;
  float joycon_left_y;
  float joycon_right_x;
  uint32_t time_ms;
  bool valid;
} app_car_command_t;

typedef struct app_car_state_s {
  volatile app_car_boot_state_t boot_state;
  const app_car_dependencies_t *dependencies_h;
  uint32_t state_started_ms;
  uint32_t config_done_ms;
  uint32_t still_since_ms;
  bool still_tracking;
  uint32_t zero_done_ms;
  uint32_t last_telemetry_ms;
  uint32_t last_log_ms;
  volatile uint32_t control_heartbeat_ms;
  volatile bool control_heartbeat_valid;
  volatile bool imu_ready;
  volatile bool faulted;
  volatile bool estop_latched;
  volatile bool motors_enabled;
  app_car_motion_t motion;
  app_car_command_t command;
  bool initialized;
} app_car_state_t;

/**
 * @brief 切换启动状态并重置静止检测状态。
 * @param state_h App 状态，不得为 NULL。
 * @param state 目标启动状态，必须为合法枚举值。
 * @param now_ms 新状态开始时间，单位 ms。
 * @post 启动状态、状态开始时间和静止检测状态已更新。
 * @note 只允许输入任务调用。
 */
void app_car_enter_state(app_car_state_t *state_h, app_car_boot_state_t state, uint32_t now_ms);

/**
 * @brief 在临界区内清除当前运动命令。
 * @param state_h App 状态，不得为 NULL。
 * @post 命令有效位清零，twist 所有分量置零。
 * @note 只修改共享命令快照，不直接操作电机。
 */
void app_car_clear_command(app_car_state_t *state_h);

/**
 * @brief 在临界区内发布运动命令和输入快照。
 * @param state_h App 状态，不得为 NULL。
 * @param input 输入快照，不得为 NULL。
 * @param twist 车体系速度命令，不得为 NULL。
 * @param now_ms 命令时间，单位 ms。
 * @post 命令快照被整体覆盖并标记有效。
 * @note 使用 System 临界区保证跨任务快照一致。
 */
void app_car_set_command(app_car_state_t *state_h, const joycon_input_t *input,
                         const chassis_twist_t *twist, uint32_t now_ms);

/**
 * @brief 在临界区内复制当前运动命令。
 * @param state_h App 状态，不得为 NULL。
 * @return 当前命令快照，内容可能无效。
 * @post 不修改 App 状态。
 * @note 返回值是值副本，不保留内部指针。
 */
app_car_command_t app_car_get_command(app_car_state_t *state_h);

/**
 * @brief 首次故障时锁存故障并记录日志。
 * @param state_h App 状态，不得为 NULL。
 * @param reason 故障原因文本，不得为 NULL。
 * @post 首次调用置位故障标志并输出错误日志。
 * @note 不直接停止电机；调用方负责执行对应的安全动作。
 */
void app_car_set_fault(app_car_state_t *state_h, const char *reason);

/**
 * @brief 请求底盘常规停止并清除运动命令。
 * @param state_h App 状态，不得为 NULL。
 * @post 必要时请求底盘制动，电机使能和急停锁存状态清零。
 * @note 只允许控制任务调用。
 */
void app_car_stop_motion(app_car_state_t *state_h);

/**
 * @brief 请求底盘急停并清除运动命令。
 * @param state_h App 状态，不得为 NULL。
 * @post 首次调用请求底盘急停；命令快照保持无效。
 * @note 允许控制或监控任务调用，底盘写操作的并发规则由调用链保证。
 */
void app_car_estop(app_car_state_t *state_h);

/**
 * @brief 检查全部电机是否处于使能状态。
 * @param state_h App 状态，不得为 NULL。
 * @retval true 全部电机均使能。
 * @retval false 任一电机未使能。
 * @post 只读取 Motion 和电机状态。
 */
bool app_car_motors_are_enabled(const app_car_state_t *state_h);

/**
 * @brief 使能全部电机并回读状态。
 * @param state_h App 状态，不得为 NULL。
 * @post Motion 已请求全部电机使能，App 使能标志反映回读结果。
 * @note 回读失败时锁存电机使能故障。
 */
void app_car_enable_motors(app_car_state_t *state_h);

/**
 * @brief 检查 IMU yaw/gyro-Z 有效性、yaw 新鲜度和通道时间戳条件。
 * @param sample IMU 快照，不得为 NULL。
 * @param now_ms 当前时间，单位 ms。
 * @retval true 必需通道有效且 yaw 未超时。
 * @retval false 通道缺失或 yaw 过期。
 * @post 不修改 IMU 或样本。
 * @note 时间差按无符号回绕处理。
 */
bool app_car_imu_yaw_is_fresh(const imu_sample_t *sample, uint32_t now_ms);

/**
 * @brief 推进 IMU、输入设备和启动状态机。
 * @param state_h App 状态，不得为 NULL。
 * @param now_ms 当前时间，单位 ms。
 * @post 设备帧和启动状态机已推进。
 * @note 只允许输入任务调用。
 */
void app_car_process_inputs(app_car_state_t *state_h, uint32_t now_ms);

/**
 * @brief 推进一次输入映射、底盘解算和电机闭环。
 * @param state_h App 状态，不得为 NULL。
 * @param now_ms 当前时间，单位 ms。
 * @post 控制心跳、安全判断、底盘目标和电机输出已推进。
 * @note 只允许控制任务调用。
 */
void app_car_process_control(app_car_state_t *state_h, uint32_t now_ms);

/**
 * @brief 推进控制心跳检查和故障安全处理。
 * @param state_h App 状态，不得为 NULL。
 * @param now_ms 当前时间，单位 ms。
 * @post 心跳超时或已有故障时已锁存故障并请求急停。
 * @note 只允许监控任务调用。
 */
void app_car_process_safety(app_car_state_t *state_h, uint32_t now_ms);

/**
 * @brief 根据有效电机数量构造遥测有效位掩码。
 * @param motor_count 有效电机数量，范围 0..APP_CAR_MOTOR_CAPACITY。
 * @return 遥测通道有效位掩码；数量越界时返回 0。
 * @post 未使用电机的目标轮速和实测轮速位被清除。
 * @note 纯函数。
 */
uint32_t app_car_build_telemetry_mask(uint8_t motor_count);

/**
 * @brief 按周期组装并提交车辆遥测。
 * @param state_h App 状态，不得为 NULL。
 * @param now_ms 样本时间，单位 ms。
 * @post 到达周期时提交一帧遥测。
 * @note 发送忙时整帧丢弃，不等待 DMA。
 */
void app_car_process_telemetry(app_car_state_t *state_h, uint32_t now_ms);

/**
 * @brief 按周期输出车辆状态日志。
 * @param state_h App 状态，不得为 NULL。
 * @param now_ms 当前时间，单位 ms。
 * @post 到达日志周期时输出一条状态日志。
 * @note 不修改车辆控制状态。
 */
void app_car_process_log(app_car_state_t *state_h, uint32_t now_ms);

/** @endcond */

#endif // LAYERED_EMBEDDED_C_APP_CAR_INTERNAL_H
