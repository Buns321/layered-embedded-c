//
// 由 Buns_ 创建于 2026/10/26。
//

/**
 * @file app_car_config.h
 * @brief F407 四轮全向小车的板级配置与待标定常数。
 * @addtogroup app
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_APP_CAR_CONFIG_H
#define LAYERED_EMBEDDED_C_APP_CAR_CONFIG_H

/** @brief 轮速 PID 控制周期（s），必须与 ControlTask 调度周期一致。 */
#define APP_CAR_CONTROL_PERIOD_S 0.01f

/** @brief JustFloat 遥测通道数。 */
#define APP_CAR_TELEMETRY_CHANNEL_COUNT 16u

/** @brief JustFloat 发送周期（ms）。 */
#define APP_CAR_TELEMETRY_PERIOD_MS 20u

/** @brief 周期状态日志周期（ms）。 */
#define APP_CAR_LOG_PERIOD_MS 1000u

/** @brief JoyCon 命令失效超时（ms）。 */
#define APP_CAR_COMMAND_TIMEOUT_MS 300u

/** @brief WHT101 yaw 数据失效超时（ms）。 */
#define APP_CAR_IMU_TIMEOUT_MS 250u

/** @brief ControlTask 心跳超时（ms）。 */
#define APP_CAR_HEARTBEAT_TIMEOUT_MS 100u

/** @brief WHT101 配置状态机超时（ms）。 */
#define APP_CAR_IMU_CONFIG_TIMEOUT_MS 3000u

/** @brief 等待 WHT101 静止的时间上限（ms）。 */
#define APP_CAR_IMU_STILL_TIMEOUT_MS 5000u

/** @brief 判定静止所需的连续时间（ms）。 */
#define APP_CAR_IMU_STILL_CONFIRM_MS 400u

/** @brief 判定静止时 gyro-Z 的绝对值上限（deg/s），待标定。 */
#define APP_CAR_IMU_STILL_GYRO_DPS 3.0f

/** @brief 等待传感器归零后新角度帧的超时（ms）。 */
#define APP_CAR_IMU_ZERO_TIMEOUT_MS 2000u

/** @brief 传感器归零后允许的残余 yaw 绝对值（deg），待标定。 */
#define APP_CAR_IMU_ZERO_RESIDUAL_DEG 5.0f

/** @brief 整车世界系最大平动速度（m/s），待标定。 */
#define APP_CAR_MAX_LINEAR_MPS 0.30f

/** @brief 整车最大自转角速度（rad/s），待标定。 */
#define APP_CAR_MAX_ANGULAR_RADPS 1.50f

/** @brief 归一化输入死区，范围为 [0, 1]，待标定。 */
#define APP_CAR_COMMAND_DEADBAND_NORM 0.03f

/** @brief 单轮目标速度上限（m/s），应覆盖平动与自转叠加。 */
#define APP_CAR_MAX_WHEEL_MPS 1.00f

/** @brief 轮速 PID 比例增益（duty / (m/s)），待整定。 */
#define APP_CAR_PID_KP 1000.0f

/** @brief 轮速 PID 积分增益（duty / (m/s * s)），待整定。 */
#define APP_CAR_PID_KI 2000.0f

/** @brief 轮速 PID 微分增益（duty * s / (m/s)），待整定。 */
#define APP_CAR_PID_KD 5.0f

/** @brief PID 积分项绝对值上限（duty）。 */
#define APP_CAR_PID_MAX_INTEGRAL 400.0f

/** @brief PID 微分一阶低通时间常数（s）。 */
#define APP_CAR_PID_DERIVATIVE_TAU_S 0.02f

/** @} */

#endif // LAYERED_EMBEDDED_C_APP_CAR_CONFIG_H
