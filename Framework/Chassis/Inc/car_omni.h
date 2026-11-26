/**
 * @file car_omni.h
 * @brief 一键装配四轮全向小车门面（支持 CAR_OMNI_APP_VIEW 隔离 HAL/BSP 依赖）。
 * @details 本文件属于 Framework/Chassis 层，静态聚合四轮编码器、PID 控制器、TB6612 电机、
 *          全向运动学、底盘、可选 WHT101 IMU、JoyCon 遥控、遥测与 3 路 Sys_Daemon 看门狗。
 *          当应用层定义 CAR_OMNI_APP_VIEW=1 时，仅暴露不透明指针与控制/步进接口，
 *          从而严格阻断 User/app_*.c 对 HAL/BSP/FreeRTOS 头文件的传递包含。
 *
 * @addtogroup Framework_Chassis
 * @{
 */

#ifndef LAYERED_CAR_CAR_OMNI_H
#define LAYERED_CAR_CAR_OMNI_H

#include "chassis_base.h"
#include "imu_base.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 整车运行安全状态（CAR_STATE_FAULT 须重新 Car_Omni_Init 方可清除）。
 */
typedef enum car_state_e {
  CAR_STATE_BOOTING = 0U, /**< 上电启动阶段（例如等待 WHT101 自动归零完成）。 */
  CAR_STATE_READY,        /**< 就绪待命状态（允许接收速度指令，电机当前未输出或已停机）。 */
  CAR_STATE_RUNNING,      /**< 正常运行状态（电机已使能且正在执行闭环速度控制）。 */
  CAR_STATE_FAULT         /**< 故障锁存状态（已触发 EStop 断电，须重新 Init 才能恢复）。 */
} car_state_t;

/** @brief 整车顶层装配配置不透明前向声明。 */
typedef struct car_omni_cfg_s car_omni_cfg_t;

/** @brief 整车静态聚合实例不透明前向声明。 */
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

/**
 * @brief 单轮硬件引脚、编码器、运动学几何与 PID 闭环配置。
 */
typedef struct car_omni_wheel_cfg_s {
  bsp_pwm_t pwm;             /**< 该轮电机 PWM 输出通道配置。 */
  bsp_encoder_cfg_t encoder; /**< 该轮正交编码器定时器与极性配置。 */
  bsp_gpio_t in1;            /**< TB6612 方向引脚 IN1 GPIO 映射。 */
  bsp_gpio_t in2;            /**< TB6612 方向引脚 IN2 GPIO 映射。 */
  bsp_gpio_t stby;           /**< TB6612 待机引脚 STBY GPIO 映射（多轮可共用同一引脚）。 */
  float theta_rad;           /**< 轮子滚动正方向角（单位：rad，以车头 x 轴为 0，逆时针为正）。 */
  float sign;             /**< 安装方向符号（+1.0f 或 -1.0f；传入 0.0f 时由运动学默认为 +1.0f）。 */
  float counts_per_meter; /**< 编码器每米线位移计数（单位：counts/m，含减速比与轮径，须 > 0）。 */
  ctrl_pid_cfg_t pid;     /**< 该轮速度闭环 PID 参数配置（目标与输出单位均对齐电机接口）。 */
} car_omni_wheel_cfg_t;

/**
 * @brief WHT101 可选串口 IMU 配置（huart == NULL 表示不启用）。
 */
typedef struct car_omni_imu_cfg_s {
  UART_HandleTypeDef *huart; /**< 绑定 WHT101 的 CubeMX UART 句柄（NULL 表示裁剪 IMU）。 */
  uint32_t timeout_ms;    /**< IMU 数据离线看门狗超时（单位：ms，0 时默认 250 ms，须 < 2^31）。 */
  bool auto_zero_on_boot; /**< 上电是否自动执行 WHT101 偏航角归零（true 时启动进入 BOOTING）。 */
} car_omni_imu_cfg_t;

/**
 * @brief JoyCon 可选串口遥控配置（huart == NULL 表示不启用）。
 */
typedef struct car_omni_rc_cfg_s {
  UART_HandleTypeDef *huart; /**< 绑定 JoyCon 的 CubeMX UART 句柄（NULL 表示裁剪遥控）。 */
  uint32_t
      timeout_ms; /**< 遥控离线看门狗与指令过期超时（单位：ms，0 时默认 300 ms，须 < 2^31）。 */
  float deadband_norm; /**< 摇杆归一化死区半径（无量纲 [0.0, 1.0)，0 时默认 0.03f）。 */
} car_omni_rc_cfg_t;

/**
 * @brief 可选串口遥测配置（huart == NULL 表示不启用）。
 */
typedef struct car_omni_telemetry_cfg_s {
  UART_HandleTypeDef *huart; /**< 绑定遥测发送的 CubeMX UART 句柄（NULL 表示裁剪遥测）。 */
  uint32_t period_ms;        /**< 遥测发送周期（单位：ms，0 时默认 20 ms 即 50 Hz）。 */
  bool use_firewater; /**< 协议选择：true 使用 FireWater 文本，false 使用 JustFloat 二进制。 */
} car_omni_telemetry_cfg_t;

/**
 * @brief 一键四轮全向车顶层配置（值为 0 的可选字段自动采用安全默认值）。
 */
typedef struct car_omni_cfg_s {
  float control_period_s;  /**< 控制闭环周期（单位：s，必填 > 0，须与 RTOS 控制任务周期一致）。 */
  float wheel_distance_m;  /**< 轮心到车体几何中心距离（单位：m，必填 > 0）。 */
  float max_linear_mps;    /**< 合成平动速度上限（单位：m/s，0 时默认 0.30 m/s）。 */
  float max_angular_radps; /**< 绕 z 轴角速度上限（单位：rad/s，0 时默认 1.50 rad/s）。 */
  chassis_frame_t drive_frame; /**< 驾驶参考坐标系（BODY 或 WORLD；WORLD 须启用 IMU）。 */
  car_omni_wheel_cfg_t wheels[KINEMATICS_OMNI_WHEELS]; /**< 4 个轮子的硬件与控制参数配置表。 */
  car_omni_imu_cfg_t imu_wht101;                       /**< 可选 WHT101 IMU 配置。 */
  car_omni_rc_cfg_t rc_joycon;                         /**< 可选 JoyCon 遥控配置。 */
  car_omni_telemetry_cfg_t telemetry;                  /**< 可选串口遥测配置。 */
  uint32_t
      control_heartbeat_timeout_ms; /**< 控制任务心跳看门狗超时（单位：ms，0 时默认 100 ms）。 */
} car_omni_cfg_t;

/**
 * @brief 遥测实例静态复用联合体（JustFloat 与 FireWater 二选一，节省静态 RAM）。
 */
typedef union car_omni_telem_u {
  telem_justfloat_t justfloat; /**< JustFloat 二进制遥测实例。 */
  telem_firewater_t firewater; /**< FireWater 文本遥测实例。 */
} car_omni_telem_t;

/**
 * @brief 一键四轮全向车静态聚合对象（须分配于普通 SRAM，严禁置于 CCMRAM）。
 */
typedef struct car_omni_s {
  bsp_encoder_t encoders[KINEMATICS_OMNI_WHEELS]; /**< 四路编码器硬件实例。 */
  controller_pid_t pids[KINEMATICS_OMNI_WHEELS];  /**< 四路轮速 PID 控制器实例。 */
  motor_tb6612_t motors[KINEMATICS_OMNI_WHEELS];  /**< 四路 TB6612 闭环电机实例。 */
  kinematics_omni_t kinematics;                   /**< 四轮全向运动学逆解实例。 */
  chassis_omni_t chassis;                         /**< 四轮全向底盘实例。 */
  filter_passthrough_t imu_filter;                /**< IMU 透传滤波管线实例。 */
  imu_wht101_t imu;                               /**< WHT101 IMU 驱动实例。 */
  rc_joycon_t rc;                                 /**< JoyCon 遥控驱动实例。 */
  car_omni_telem_t telem;                         /**< 遥测驱动联合体存储。 */
  telemetry_base_t *telemetry_h; /**< 指向有效遥测基类的指针（未启用时为 NULL）。 */
  sys_daemon_t imu_daemon;       /**< IMU 离线监控看门狗节点。 */
  sys_daemon_t rc_daemon;        /**< 遥控离线监控看门狗节点。 */
  sys_daemon_t ctrl_daemon;      /**< 控制任务心跳看门狗节点。 */
  volatile car_state_t state;    /**< 整车当前状态机状态（跨任务/ISR 可见）。 */
  chassis_twist_t desired_twist; /**< 经限幅后的期望速度快照（由 SYS_SNAPSHOT 保护）。 */
  uint32_t last_command_ms;      /**< 最近一次收到有效速度指令的时间戳（单位：ms）。 */
  uint32_t last_telemetry_ms;    /**< 最近一次发送遥测的时间戳（单位：ms）。 */
  uint32_t last_log_ms;          /**< 最近一次输出周期状态日志的时间戳（单位：ms）。 */
  uint32_t last_imu_seq;         /**< 上次喂狗时记录的 IMU 样本序号（防重复喂狗）。 */
  float control_period_s;        /**< 缓存的控制周期（单位：s）。 */
  float max_linear_mps;          /**< 缓存的线速度圆上限（单位：m/s）。 */
  float max_angular_radps;       /**< 缓存的角速度上限（单位：rad/s）。 */
  float rc_deadband_norm;        /**< 缓存的遥控死区阈值（无量纲 [0.0, 1.0)）。 */
  chassis_frame_t drive_frame;   /**< 缓存的底盘参考坐标系。 */
  uint32_t imu_timeout_ms;       /**< 缓存的 IMU 超时阈值（单位：ms）。 */
  uint32_t rc_timeout_ms;        /**< 缓存的遥控超时阈值（单位：ms）。 */
  uint32_t telemetry_period_ms;  /**< 缓存的遥测发送周期（单位：ms）。 */
  uint32_t ctrl_hb_timeout_ms;   /**< 缓存的控制心跳超时阈值（单位：ms）。 */
  bool initialized;              /**< 整车静态装配完成标志（true 表示全部子系统就绪）。 */
  bool has_imu;                  /**< 是否已启用 WHT101 IMU。 */
  bool has_rc;                   /**< 是否已启用 JoyCon 遥控。 */
  bool has_telemetry;            /**< 是否已启用串口遥测。 */
  bool motors_enabled;           /**< 四路电机当前是否处于使能状态。 */
  bool estop_latched;            /**< 急停锁存标志（置位后仅重新 Init 可清除）。 */
  bool has_command;              /**< 当前是否存在有效非零运动指令请求。 */
} car_omni_t;

/**
 * @brief 一键初始化四轮全向车全部静态子系统并注册紧急断电钩子。
 * @param[out] car 待初始化的整车静态聚合对象指针（不可为 NULL，须位于普通 SRAM）。
 * @param[in]  cfg 顶层配置指针（不可为 NULL）。
 * @return 初始化结果状态码。
 * @retval BSP_OK              全部子系统装配完成并注册 RTOS_SetEmergencyHaltHook。
 * @retval BSP_ERR_INVALID_ARG 配置参数非法、WORLD 模式未提供 IMU 或运动学/PID 校验失败。
 * @retval BSP_ERR_HAL         底盘/外设（编码器、电机、IMU、RC、遥测）底层初始化失败。
 * @pre 须在 CubeMX 外设初始化（MX_*_Init）完成后、osKernelStart() 启动前调用。
 * @note 若开启 imu_wht101.auto_zero_on_boot，初始状态为 CAR_STATE_BOOTING，否则为 CAR_STATE_READY。
 */
bsp_status_t Car_Omni_Init(car_omni_t *car, const car_omni_cfg_t *cfg);

#endif /* !CAR_OMNI_APP_VIEW */

/**
 * @brief 推进 IMU 与 RC 输入接收解析并检查上电归零故障。
 * @param[in,out] car    整车门面句柄（为 NULL 或未初始化时直接返回）。
 * @param[in]     now_ms 当前系统单调毫秒时间戳（单位：ms）。
 * @pre 须在 Car_Omni_Init() 成功返回后由输入周期任务（如 2 ms）调用。
 * @note 首次执行时按需向 Sys_Daemon 注册 rc_daemon 与 imu_daemon；若 BOOTING 期间
 *       WHT101 报告故障，立即触发 Car_Omni_EStop()。
 */
void Car_Omni_ProcessInputs(car_omni_t *car, uint32_t now_ms);

/**
 * @brief 下发车体/世界系速度命令并执行速度圆与角速度限幅。
 * @param[in,out] car    整车门面句柄（为 NULL 或未初始化时直接返回）。
 * @param[in]     twist  期望速度命令指针（为 NULL 时直接返回，NaN/Inf 分量自动清零）。
 * @param[in]     now_ms 当前系统单调毫秒时间戳（单位：ms）。
 * @note 当整车处于 CAR_STATE_BOOTING 或 CAR_STATE_FAULT 时忽略指令；线速度矢量模长超过
 *       max_linear_mps 时按比例缩放，角速度钳位至 [-max_angular_radps, +max_angular_radps]，
 *       并通过 SYS_SNAPSHOT_WRITE 原子写入 desired_twist。
 */
void Car_Omni_SetTwist(car_omni_t *car, const chassis_twist_t *twist, uint32_t now_ms);

/**
 * @brief 从 JoyCon 快照读取摇杆输入、应用死区/超时检查并更新速度命令。
 * @param[in,out] car    整车门面句柄（为 NULL、未初始化或未启用 RC 时直接返回）。
 * @param[in]     now_ms 当前系统单调毫秒时间戳（单位：ms）。
 * @note 若遥控数据未就绪、超时或摇杆均在死区内，自动调用 Car_Omni_Stop()；处于 BOOTING
 *       或 FAULT 状态时直接返回。
 */
void Car_Omni_DriveFromRC(car_omni_t *car, uint32_t now_ms);

/**
 * @brief 运行一次控制心跳喂狗、运动学更新与四轮闭环控制。
 * @param[in,out] car    整车门面句柄（为 NULL 或未初始化时直接返回）。
 * @param[in]     now_ms 当前系统单调毫秒时间戳（单位：ms）。
 * @pre 须由高优先级控制任务按固定周期（须与 control_period_s 一致）调用。
 * @note 每次调用均向 ctrl_daemon 喂狗；若任一轮控制器返回 CONTROLLER_STATUS_ERROR 或电机
 *       使能失败，立即触发 Car_Omni_EStop() 并锁存 CAR_STATE_FAULT。
 */
void Car_Omni_StepControl(car_omni_t *car, uint32_t now_ms);

/**
 * @brief 推进 Daemon 看门狗监控、16 通道遥测与周期状态日志。
 * @param[in,out] car    整车门面句柄（为 NULL 或未初始化时直接返回）。
 * @param[in]     now_ms 当前系统单调毫秒时间戳（单位：ms）。
 * @note 内部调用 Sys_Daemon_Poll(now_ms)，建议由低优先级服务任务周期驱动。
 */
void Car_Omni_StepService(car_omni_t *car, uint32_t now_ms);

/**
 * @brief 常规制动停机（不进入 FAULT，可恢复运行）。
 * @param[in,out] car 整车门面句柄（为 NULL 或未初始化时直接返回）。
 * @note 清零 desired_twist 快照并置 has_command=false；若当前处于 CAR_STATE_RUNNING，
 *       将状态迁回 CAR_STATE_READY；若已处于 CAR_STATE_FAULT 则保持故障态不变。
 */
void Car_Omni_Stop(car_omni_t *car);

/**
 * @brief 紧急断电停机并永久锁存 CAR_STATE_FAULT。
 * @param[in,out] car    整车门面句柄（为 NULL 或未初始化时直接返回）。
 * @param[in]     reason 故障原因描述字符串（允许为 NULL，默认记录 "estop"）。
 * @warning 本函数通过临界区幂等锁存 estop_latched 与 CAR_STATE_FAULT，并调用
 *          Chassis_Base_EStop() 切断电机输出；退出 FAULT 唯一途径是重新调用 Car_Omni_Init()。
 */
void Car_Omni_EStop(car_omni_t *car, const char *reason);

/**
 * @brief 查询整车当前状态机状态。
 * @param[in] car 整车门面只读句柄（允许为 NULL）。
 * @return 当前整车状态枚举值。
 * @retval CAR_STATE_FAULT 当 car 为 NULL 或整车已处于故障锁存态时返回。
 */
car_state_t Car_Omni_GetState(const car_omni_t *car);

/**
 * @brief 获取底盘多态基类视图。
 * @param[in,out] car 整车门面句柄（允许为 NULL）。
 * @return 指向内部 chassis.base 的指针；当 car 为 NULL 或未初始化时返回 NULL。
 */
chassis_base_t *Car_Omni_Chassis(car_omni_t *car);

/**
 * @brief 获取 IMU 多态基类视图。
 * @param[in,out] car 整车门面句柄（允许为 NULL）。
 * @return 指向内部 imu.base 的指针；当 car 为 NULL、未初始化或未启用 IMU 时返回 NULL。
 */
imu_base_t *Car_Omni_Imu(car_omni_t *car);

/** @} */

#endif /* LAYERED_CAR_CAR_OMNI_H */
