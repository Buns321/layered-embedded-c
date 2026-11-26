/**
 * @file car_omni.h
 * @brief 一键装配四轮全向小车门面（支持 CAR_OMNI_APP_VIEW 隔离 HAL/BSP 依赖）。
 * @addtogroup chassis
 * @{
 */

#ifndef LAYERED_CAR_CAR_OMNI_H
#define LAYERED_CAR_CAR_OMNI_H

#include "chassis_base.h"
#include "imu_base.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 整车运行安全状态（FAULT 需重新 Init 方可清除）。
 */
typedef enum car_state_e {
  CAR_STATE_BOOTING = 0U,
  CAR_STATE_READY,
  CAR_STATE_RUNNING,
  CAR_STATE_FAULT
} car_state_t;

typedef struct car_omni_cfg_s car_omni_cfg_t;
typedef struct car_omni_s car_omni_t;

#if !defined(CAR_OMNI_APP_VIEW)

#include "bsp_encoder.h"
#include "chassis_omni.h"
#include "controller_pid.h"
#include "filter_passthrough.h"
#include "imu_wht101.h"
#include "kinematics_omni.h"
#include "motor_tb6612.h"
#include "rc_joycon.h"
#include "sys_daemon.h"
#include "telem_firewater.h"
#include "telem_justfloat.h"

/** @brief 单轮硬件、运动学与 PID 闭环配置。 */
typedef struct car_omni_wheel_cfg_s {
  bsp_pwm_t pwm;
  bsp_encoder_cfg_t encoder;
  bsp_gpio_t in1;
  bsp_gpio_t in2;
  bsp_gpio_t stby;
  float theta_rad;
  float sign; /* +1.0f / -1.0f，0 默认 +1.0f */
  float counts_per_meter;
  ctrl_pid_cfg_t pid;
} car_omni_wheel_cfg_t;

/** @brief WHT101 可选 IMU 配置（huart == NULL 表示不启用）。 */
typedef struct car_omni_imu_cfg_s {
  UART_HandleTypeDef *huart;
  uint32_t timeout_ms; /* 默认 250 ms */
  bool auto_zero_on_boot;
} car_omni_imu_cfg_t;

/** @brief JoyCon 可选串口遥控配置（huart == NULL 表示不启用）。 */
typedef struct car_omni_rc_cfg_s {
  UART_HandleTypeDef *huart;
  uint32_t timeout_ms; /* 默认 300 ms */
  float deadband_norm; /* 默认 0.03 */
} car_omni_rc_cfg_t;

/** @brief 可选遥测配置（huart == NULL 表示不启用）。 */
typedef struct car_omni_telemetry_cfg_s {
  UART_HandleTypeDef *huart;
  uint32_t period_ms; /* 默认 20 ms */
  bool use_firewater;
} car_omni_telemetry_cfg_t;

/**
 * @brief 一键四轮全向车顶层配置（0 值可选字段采用安全默认值）。
 */
typedef struct car_omni_cfg_s {
  float control_period_s;  /* 必填 > 0，单位 s */
  float wheel_distance_m;  /* 必填 > 0，单位 m */
  float max_linear_mps;    /* 默认 0.30 m/s */
  float max_angular_radps; /* 默认 1.50 rad/s */
  chassis_frame_t drive_frame;
  car_omni_wheel_cfg_t wheels[KINEMATICS_OMNI_WHEELS];
  car_omni_imu_cfg_t imu_wht101;
  car_omni_rc_cfg_t rc_joycon;
  car_omni_telemetry_cfg_t telemetry;
  uint32_t control_heartbeat_timeout_ms; /* 默认 100 ms */
} car_omni_cfg_t;

typedef union car_omni_telem_u {
  telem_justfloat_t justfloat;
  telem_firewater_t firewater;
} car_omni_telem_t;

/**
 * @brief 一键四轮全向车静态聚合对象（须位于普通 SRAM）。
 */
typedef struct car_omni_s {
  bsp_encoder_t encoders[KINEMATICS_OMNI_WHEELS];
  controller_pid_t pids[KINEMATICS_OMNI_WHEELS];
  motor_tb6612_t motors[KINEMATICS_OMNI_WHEELS];
  kinematics_omni_t kinematics;
  chassis_omni_t chassis;
  filter_passthrough_t imu_filter;
  imu_wht101_t imu;
  rc_joycon_t rc;
  car_omni_telem_t telem;
  telemetry_base_t *telemetry_h;
  sys_daemon_t imu_daemon;
  sys_daemon_t rc_daemon;
  sys_daemon_t ctrl_daemon;
  volatile car_state_t state;
  chassis_twist_t desired_twist;
  uint32_t last_command_ms;
  uint32_t last_telemetry_ms;
  uint32_t last_log_ms;
  uint32_t last_imu_seq;
  float control_period_s;
  float max_linear_mps;
  float max_angular_radps;
  float rc_deadband_norm;
  chassis_frame_t drive_frame;
  uint32_t imu_timeout_ms;
  uint32_t rc_timeout_ms;
  uint32_t telemetry_period_ms;
  uint32_t ctrl_hb_timeout_ms;
  bool initialized;
  bool has_imu;
  bool has_rc;
  bool has_telemetry;
  bool motors_enabled;
  bool estop_latched;
  bool has_command;
} car_omni_t;

/**
 * @brief 一键初始化四轮全向车全部静态子系统并注册紧急断电钩子。
 */
bsp_status_t Car_Omni_Init(car_omni_t *car, const car_omni_cfg_t *cfg);

#endif /* !CAR_OMNI_APP_VIEW */

/** @brief 推进 IMU 与 RC 输入接收解析并检查上电归零故障（输入任务调用）。 */
void Car_Omni_ProcessInputs(car_omni_t *car, uint32_t now_ms);

/** @brief 下发车体/世界系速度命令并执行速度圆与角速度限幅。 */
void Car_Omni_SetTwist(car_omni_t *car, const chassis_twist_t *twist, uint32_t now_ms);

/** @brief 从 JoyCon 快照读取摇杆输入、应用死区/超时检查并更新速度命令。 */
void Car_Omni_DriveFromRC(car_omni_t *car, uint32_t now_ms);

/** @brief 运行一次控制心跳喂狗、运动学更新与四轮闭环控制（控制任务调用）。 */
void Car_Omni_StepControl(car_omni_t *car, uint32_t now_ms);

/** @brief 推进 Daemon 看门狗监控、16 通道遥测与周期状态日志（服务任务调用）。 */
void Car_Omni_StepService(car_omni_t *car, uint32_t now_ms);

/** @brief 常规制动停机（不进入 FAULT，可恢复运行）。 */
void Car_Omni_Stop(car_omni_t *car);

/** @brief 紧急断电停机并永久锁存 CAR_STATE_FAULT。 */
void Car_Omni_EStop(car_omni_t *car, const char *reason);

/** @brief 查询整车当前状态。 */
car_state_t Car_Omni_GetState(const car_omni_t *car);

/** @brief 获取底盘多态基类视图（未初始化返回 NULL）。 */
chassis_base_t *Car_Omni_Chassis(car_omni_t *car);

/** @brief 获取 IMU 多态基类视图（未启用 IMU 返回 NULL）。 */
imu_base_t *Car_Omni_Imu(car_omni_t *car);

/** @} */

#endif /* LAYERED_CAR_CAR_OMNI_H */
