//
// 由 Buns_ 创建于 2026/10/23。
//

/**
 * @file mpu6050_i2c.h
 * @brief MPU6050 I2C 驱动、原始样本与统一 IMU 滤波管线适配。
 * @addtogroup drivers_concrete
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_MPU6050_I2C_H
#define LAYERED_EMBEDDED_C_MPU6050_I2C_H

#include "imu_base.h"
#include "platform_i2c.h"
#include <stdbool.h>
#include <stdint.h>

#define MPU6050_ADDRESS_AD0_LOW    0x68u
#define MPU6050_ADDRESS_AD0_HIGH   0x69u
#define MPU6050_WHO_AM_I_VALUE     0x68u
#define MPU6050_WHO_AM_I_FIELD     0x34u

/**
 * @brief 数字低通滤波器配置。
 */
typedef enum mpu6050_dlpf_e {
  MPU6050_DLPF_256HZ = 0u,
  MPU6050_DLPF_188HZ,
  MPU6050_DLPF_98HZ,
  MPU6050_DLPF_42HZ,
  MPU6050_DLPF_20HZ,
  MPU6050_DLPF_10HZ,
  MPU6050_DLPF_5HZ,
} mpu6050_dlpf_t;

/**
 * @brief 陀螺仪量程配置。
 */
typedef enum mpu6050_gyro_range_e {
  MPU6050_GYRO_RANGE_250DPS = 0u,
  MPU6050_GYRO_RANGE_500DPS,
  MPU6050_GYRO_RANGE_1000DPS,
  MPU6050_GYRO_RANGE_2000DPS,
} mpu6050_gyro_range_t;

/**
 * @brief 加速度计量程配置。
 */
typedef enum mpu6050_accel_range_e {
  MPU6050_ACCEL_RANGE_2G = 0u,
  MPU6050_ACCEL_RANGE_4G,
  MPU6050_ACCEL_RANGE_8G,
  MPU6050_ACCEL_RANGE_16G,
} mpu6050_accel_range_t;

/**
 * @brief MPU6050 初始化配置。
 * @note axis_source 和 axis_sign 把芯片轴映射到车体轴。
 * @note address 使用 7 位地址，由 AD0 电平决定。
 */
typedef struct mpu6050_config_s {
  platform_i2c_instance_t i2c;
  uint8_t address;
  mpu6050_dlpf_t dlpf;
  mpu6050_gyro_range_t gyro_range;
  mpu6050_accel_range_t accel_range;
  uint16_t sample_rate_hz;
  uint16_t calibration_samples;
  uint32_t i2c_timeout_ms;
  uint8_t axis_source[3];
  int8_t axis_sign[3];
} mpu6050_config_t;

/**
 * @brief 未经软件滤波的工程单位样本。
 */
typedef struct mpu6050_raw_sample_s {
  float accel_mps2[3];
  float gyro_dps[3];
  float temperature_c;
  uint32_t time_ms;
} mpu6050_raw_sample_t;

/**
 * @brief MPU6050 状态机。
 */
typedef enum mpu6050_state_e {
  MPU6050_STATE_UNINITIALIZED = 0u,
  MPU6050_STATE_RESET_PENDING,
  MPU6050_STATE_RESET_WAIT,
  MPU6050_STATE_CALIBRATE_PENDING,
  MPU6050_STATE_CALIBRATING,
  MPU6050_STATE_READY,
  MPU6050_STATE_ERROR,
} mpu6050_state_t;

/**
 * @brief MPU6050 具体对象。
 * @note imu_base 必须是首成员。
 */
typedef struct mpu6050_i2c_s {
  imu_base_t imu_base;
  platform_i2c_instance_t i2c;
  uint8_t address;
  mpu6050_config_t config;

  mpu6050_state_t state;
  bool is_started;
  uint32_t state_deadline_ms;
  uint32_t next_sample_ms;
  uint32_t sample_period_ms;
  uint8_t sample_rate_divider;

  float accel_lsb_per_g;
  float gyro_lsb_per_dps;
  float gyro_bias_dps[3];

  uint32_t calibration_count;
  float calibration_gyro_sum[3];
  float calibration_gyro_sum_sq[3];

  mpu6050_raw_sample_t raw_sample;
  volatile bool has_raw_sample;
  volatile bool last_request_failed;
  volatile uint32_t i2c_error_count;
  volatile uint32_t configuration_error_count;
  volatile uint32_t calibration_error_count;
  volatile uint32_t filter_reject_count;
} mpu6050_i2c_t;

/**
 * @brief 初始化并探测 MPU6050。
 * @param imu_h 对象。
 * @param name 名称。
 * @param config 配置，内容会被复制。
 * @param filter_h 已初始化的静态滤波器根节点。
 * @retval PLATFORM_STATUS_OK WHO_AM_I 匹配。
 * @retval PLATFORM_STATUS_ERROR 参数、通信或设备 ID 错误。
 * @retval PLATFORM_STATUS_TIMEOUT I2C 超时。
 * @note 本函数执行一次阻塞 WHO_AM_I 读取，不启动采样。
 */
platform_status_t Dev_MPU6050_I2C_Init(
  mpu6050_i2c_t *imu_h,
  const char *name,
  const mpu6050_config_t *config,
  filter_base_t *filter_h
);

/**
 * @brief 复制最近一次经单位换算和轴映射、但未经软件滤波的样本。
 * @return 是否已有可用样本。
 */
bool Dev_MPU6050_I2C_GetRawSample(
  mpu6050_i2c_t *imu_h,
  mpu6050_raw_sample_t *sample
);

/**
 * @brief 请求重新采集静止陀螺零偏。
 * @retval PLATFORM_STATUS_OK 请求已接受。
 * @retval PLATFORM_STATUS_ERROR IMU 未启动或当前状态不允许标定。
 * @note 标定期间 process() 不发布新样本。
 */
platform_status_t Dev_MPU6050_I2C_RequestGyroCalibration(
  mpu6050_i2c_t *imu_h
);

/** @} */

#endif //LAYERED_EMBEDDED_C_MPU6050_I2C_H
