/**
 * @file imu_mpu6050.h
 * @brief MPU6050 I2C 6 轴 IMU 驱动（直连 bsp_i2c_t，按 HAL_I2C_MODULE_ENABLED 启用）。
 * @addtogroup drivers
 * @{
 */

#ifndef LAYERED_CAR_IMU_MPU6050_H
#define LAYERED_CAR_IMU_MPU6050_H

#include "bsp_def.h"
#include "bsp_i2c.h"
#include "imu_base.h"
#include <stdbool.h>
#include <stdint.h>

#if defined(HAL_I2C_MODULE_ENABLED) || defined(BSP_HOST_STUB)

#define MPU6050_ADDRESS_AD0_LOW 0x68U
#define MPU6050_ADDRESS_AD0_HIGH 0x69U
#define MPU6050_WHO_AM_I_VALUE 0x68U
#define MPU6050_WHO_AM_I_ALT 0x34U

/** @brief 数字低通滤波器带宽配置。 */
typedef enum mpu6050_dlpf_e {
  MPU6050_DLPF_256HZ = 0U,
  MPU6050_DLPF_188HZ,
  MPU6050_DLPF_98HZ,
  MPU6050_DLPF_42HZ,
  MPU6050_DLPF_20HZ,
  MPU6050_DLPF_10HZ,
  MPU6050_DLPF_5HZ
} mpu6050_dlpf_t;

/** @brief 陀螺仪量程配置。 */
typedef enum mpu6050_gyro_range_e {
  MPU6050_GYRO_RANGE_250DPS = 0U,
  MPU6050_GYRO_RANGE_500DPS,
  MPU6050_GYRO_RANGE_1000DPS,
  MPU6050_GYRO_RANGE_2000DPS
} mpu6050_gyro_range_t;

/** @brief 加速度计量程配置。 */
typedef enum mpu6050_accel_range_e {
  MPU6050_ACCEL_RANGE_2G = 0U,
  MPU6050_ACCEL_RANGE_4G,
  MPU6050_ACCEL_RANGE_8G,
  MPU6050_ACCEL_RANGE_16G
} mpu6050_accel_range_t;

/** @brief MPU6050 运行状态机阶段。 */
typedef enum mpu6050_state_e {
  MPU6050_STATE_UNINITIALIZED = 0U,
  MPU6050_STATE_RESET_PENDING,
  MPU6050_STATE_RESET_WAIT,
  MPU6050_STATE_CALIBRATE_PENDING,
  MPU6050_STATE_CALIBRATING,
  MPU6050_STATE_READY,
  MPU6050_STATE_ERROR
} mpu6050_state_t;

/**
 * @brief MPU6050 初始化配置（sample_rate_hz 4..1000，axis_source 0..2 排列，axis_sign ±1）。
 */
typedef struct imu_mpu6050_cfg_s {
  const char *name;
  bsp_i2c_t i2c;
  filter_base_t *filter;
  mpu6050_dlpf_t dlpf;
  mpu6050_gyro_range_t gyro_range;
  mpu6050_accel_range_t accel_range;
  uint16_t sample_rate_hz;
  uint16_t calibration_samples;
  uint8_t axis_source[3];
  int8_t axis_sign[3];
} imu_mpu6050_cfg_t;

/**
 * @brief 轴映射与单位换算后的未滤波工程单位样本。
 */
typedef struct mpu6050_raw_sample_s {
  float accel_mps2[3];
  float gyro_dps[3];
  float temperature_c;
  uint32_t time_ms;
} mpu6050_raw_sample_t;

/**
 * @brief MPU6050 静态驱动对象（base 必须为首成员）。
 */
typedef struct imu_mpu6050_s {
  imu_base_t base;
  imu_mpu6050_cfg_t cfg;
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
} imu_mpu6050_t;

/**
 * @brief 校验配置并探测 MPU6050 WHO_AM_I 寄存器。
 */
bsp_status_t Dev_IMU_MPU6050_Init(imu_mpu6050_t *imu, const imu_mpu6050_cfg_t *cfg);

/**
 * @brief 临界区内复制最近一次未滤波的六轴与温度样本。
 */
bool Dev_IMU_MPU6050_GetRawSample(const imu_mpu6050_t *imu, mpu6050_raw_sample_t *out_sample);

/**
 * @brief 请求在 READY 状态重新采集静止陀螺零偏。
 */
bsp_status_t Dev_IMU_MPU6050_RequestGyroCalibration(imu_mpu6050_t *imu);

#endif /* HAL_I2C_MODULE_ENABLED || BSP_HOST_STUB */

/** @} */

#endif /* LAYERED_CAR_IMU_MPU6050_H */
