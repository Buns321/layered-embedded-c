//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file app_fast_channel_config.h
 * @brief F103 速通赛道任务一的板级、标定、EKF 和遥测参数。
 * @addtogroup app
 * @{
 * @note 所有可调编译期默认值集中在本文件头部；EKF 在线参数从这些默认值
 *       初始化，运行期修改不写回 Flash。
 */

#ifndef LAYERED_EMBEDDED_C_APP_FAST_CHANNEL_CONFIG_H
#define LAYERED_EMBEDDED_C_APP_FAST_CHANNEL_CONFIG_H

/**
 * @brief 板级遥测通道容量。
 * @note CMake 可显式传入相同值；本定义保证单独打开源码时 IntelliSense
 *       和静态分析也使用任务一的 32 通道配置。
 */
#ifndef TELEMETRY_CHANNEL_CAPACITY
#define TELEMETRY_CHANNEL_CAPACITY 32u
#endif

#include "mpu6050_i2c.h"
#include "platform_gpio.h"
#include "platform_i2c.h"
#include "platform_uart.h"
#include <stdint.h>

/** @brief 板级 UART 逻辑实例。 */
#define APP_UART_INSTANCE PLATFORM_UART_1

/** @brief MPU6050 I2C 逻辑实例。 */
#define APP_IMU_I2C_INSTANCE PLATFORM_I2C_1

/** @brief MPU6050 7 位地址，AD0 接地。 */
#define APP_IMU_I2C_ADDRESS MPU6050_ADDRESS_AD0_LOW

/** @brief MPU6050 数字低通滤波配置。 */
#define APP_IMU_DLPF MPU6050_DLPF_42HZ

/** @brief MPU6050 陀螺量程。 */
#define APP_IMU_GYRO_RANGE MPU6050_GYRO_RANGE_500DPS

/** @brief MPU6050 加速度量程。 */
#define APP_IMU_ACCEL_RANGE MPU6050_ACCEL_RANGE_4G

/** @brief MPU6050 目标采样率（Hz）。 */
#define APP_IMU_SAMPLE_RATE_HZ 100u

/** @brief MPU6050 单次 I2C 操作超时（ms）。 */
#define APP_IMU_I2C_TIMEOUT_MS 10u

/** @brief 片内 Flash 记录 Magic。 */
#define APP_CALIBRATION_RECORD_MAGIC 0x4643414CU

/** @brief 校准记录格式版本。 */
#define APP_CALIBRATION_RECORD_VERSION 1u

/** @brief 校准记录第一页地址，使用最后两页。 */
#define APP_CALIBRATION_FLASH_ADDRESS 0x0800F800UL

/** @brief F103 单页 Flash 大小（byte）。 */
#define APP_CALIBRATION_FLASH_PAGE_SIZE 1024UL

/** @brief IMU 任务周期（ms）。 */
#define APP_IMU_TASK_PERIOD_MS 10u

/** @brief 遥测任务周期（ms）。 */
#define APP_TELEMETRY_TASK_PERIOD_MS 20u

/** @brief IMU 任务静态栈大小（byte）。 */
#define APP_IMU_TASK_STACK_SIZE 3072u

/** @brief 初始姿态判断等待时间（ms）。 */
#define APP_INITIAL_POSE_CHECK_MS 3000u

/** @brief 每个新面的超时时间（ms）。 */
#define APP_CALIBRATION_FACE_TIMEOUT_MS 30000u

/** @brief 陀螺零偏标定采样数。 */
#define APP_GYRO_CALIBRATION_SAMPLES 100u

/** @brief 陀螺标定允许的最大标准差（deg/s）。 */
#define APP_GYRO_CALIBRATION_MAX_STD_DPS 3.0f

/** @brief 面稳定判定时间（ms）。 */
#define APP_FACE_STABLE_MS 1000u

/** @brief 单面加速度采样数。 */
#define APP_FACE_SAMPLE_COUNT 100u

/** @brief 静止判定加速度模长下限（m/s^2）。 */
#define APP_STILL_ACCEL_MIN_MPS2 7.8f

/** @brief 静止判定加速度模长上限（m/s^2）。 */
#define APP_STILL_ACCEL_MAX_MPS2 11.8f

/** @brief 静止判定三轴角速度上限（deg/s）。 */
#define APP_STILL_GYRO_MAX_DPS 3.0f

/** @brief 未标定阶段的原始角速度静止上限（deg/s）。 */
#define APP_RAW_STILL_GYRO_MAX_DPS 10.0f

/** @brief 面识别主轴最小绝对加速度（m/s^2）。 */
#define APP_FACE_DOMINANT_MIN_MPS2 6.0f

/** @brief 面识别非主轴最大绝对加速度（m/s^2）。 */
#define APP_FACE_CROSS_MAX_MPS2 5.0f

/** @brief 六面标定正负样本最小差值（m/s^2）。 */
#define APP_ACCEL_CALIBRATION_MIN_SPAN_MPS2 4.0f

/** @brief 加速度比例系数安全下限。 */
#define APP_ACCEL_SCALE_MIN 0.1f

/** @brief 加速度比例系数安全上限。 */
#define APP_ACCEL_SCALE_MAX 10.0f

/** @brief 标准重力加速度（m/s^2）。 */
#define APP_GRAVITY_MPS2 9.80665f

/** @brief 初始倒置判定 Z 轴门限（m/s^2）。 */
#define APP_INVERTED_AZ_MAX_MPS2 -7.8f

/** @brief 短鸣持续时间（ms）。 */
#define APP_BUZZER_SHORT_ON_MS 80u

/** @brief 两次短鸣间隔（ms）。 */
#define APP_BUZZER_SHORT_GAP_MS 120u

/** @brief 长鸣持续时间（ms）。 */
#define APP_BUZZER_LONG_ON_MS 600u

/** @brief 有源蜂鸣器开启电平。 */
#define APP_BUZZER_ON_LEVEL 0U

/** @brief PC13 LED 点亮电平。 */
#define APP_LED_ON_LEVEL 0U

/** @brief 蜂鸣器 GPIO 端口。 */
#define APP_BUZZER_PORT PLATFORM_GPIO_PORT_A

/** @brief 蜂鸣器 GPIO 引脚掩码。 */
#define APP_BUZZER_PIN ((uint16_t)0x0001u)

/** @brief LED GPIO 端口。 */
#define APP_LED_PORT PLATFORM_GPIO_PORT_C

/** @brief LED GPIO 引脚掩码。 */
#define APP_LED_PIN ((uint16_t)0x2000u)

/** @brief JustFloat 实际发送通道数。 */
#define APP_TELEMETRY_CHANNEL_COUNT TELEMETRY_CHANNEL_CAPACITY

/** @brief UART 参数命令行最大长度，不含换行符。 */
#define APP_UART_COMMAND_CAPACITY 64u

/** @brief UART 接收环形缓冲大小。 */
#define APP_UART_RX_RING_CAPACITY 256u

/** @brief EKF 姿态过程噪声（rad^2）。 */
#define APP_EKF_ANGLE_PROCESS_NOISE_RAD2 1.0e-4f

/** @brief EKF 零偏过程噪声（(rad/s)^2）。 */
#define APP_EKF_BIAS_PROCESS_NOISE_RADPS2 1.0e-6f

/** @brief EKF 加速度观测噪声（(m/s^2)^2）。 */
#define APP_EKF_ACCEL_MEASUREMENT_NOISE_MPS2_2 0.1f

/** @brief EKF 初始姿态方差（rad^2）。 */
#define APP_EKF_INITIAL_ANGLE_VARIANCE_RAD2 1.0f

/** @brief EKF 初始零偏方差（(rad/s)^2）。 */
#define APP_EKF_INITIAL_BIAS_VARIANCE_RADPS2 0.01f

/** @brief EKF 加速度模长下限（m/s^2）。 */
#define APP_EKF_ACCEL_NORM_MIN_MPS2 7.8f

/** @brief EKF 加速度模长上限（m/s^2）。 */
#define APP_EKF_ACCEL_NORM_MAX_MPS2 11.8f

/** @brief EKF 角速度保护门限（rad/s）。 */
#define APP_EKF_GYRO_LIMIT_RADPS 17.45f

/** @brief EKF 零偏保护门限（rad/s）。 */
#define APP_EKF_BIAS_LIMIT_RADPS 1.0f

/** @brief EKF 最小融合周期（s）。 */
#define APP_EKF_MIN_DT_S 0.001f

/** @brief EKF 最大融合周期（s）。 */
#define APP_EKF_MAX_DT_S 0.05f

/** @} */

#endif //LAYERED_EMBEDDED_C_APP_FAST_CHANNEL_CONFIG_H
