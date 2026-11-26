/**
 * @file imu_mpu6050.h
 * @brief MPU6050 I2C 6 轴 IMU 驱动接口。
 * @details 属于 Framework/Drivers 设备驱动层，直连 bsp_i2c_t，实现非阻塞复位、
 *          寄存器配置与回读校验、静止陀螺仪零偏标定以及轴映射单位换算。
 *          按 HAL_I2C_MODULE_ENABLED 或 BSP_HOST_STUB 条件编译启用。
 *
 * @addtogroup Framework_Drivers
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

/** @brief MPU6050 7 位 I2C 从机地址（AD0 引脚接地）。 */
#define MPU6050_ADDRESS_AD0_LOW 0x68U
/** @brief MPU6050 7 位 I2C 从机地址（AD0 引脚接高电平）。 */
#define MPU6050_ADDRESS_AD0_HIGH 0x69U
/** @brief MPU6050 WHO_AM_I 寄存器标准返回值。 */
#define MPU6050_WHO_AM_I_VALUE 0x68U
/** @brief MPU6050 兼容芯片 WHO_AM_I 寄存器备用返回值。 */
#define MPU6050_WHO_AM_I_ALT 0x34U

/**
 * @brief 数字低通滤波器（DLPF）带宽配置枚举。
 */
typedef enum mpu6050_dlpf_e {
  MPU6050_DLPF_256HZ = 0U, /**< 加速度 260 Hz / 陀螺 256 Hz（内部采样率 8 kHz） */
  MPU6050_DLPF_188HZ,      /**< 加速度 184 Hz / 陀螺 188 Hz（内部采样率 1 kHz） */
  MPU6050_DLPF_98HZ,       /**< 加速度 94 Hz / 陀螺 98 Hz（内部采样率 1 kHz） */
  MPU6050_DLPF_42HZ,       /**< 加速度 44 Hz / 陀螺 42 Hz（内部采样率 1 kHz） */
  MPU6050_DLPF_20HZ,       /**< 加速度 21 Hz / 陀螺 20 Hz（内部采样率 1 kHz） */
  MPU6050_DLPF_10HZ,       /**< 加速度 10 Hz / 陀螺 10 Hz（内部采样率 1 kHz） */
  MPU6050_DLPF_5HZ         /**< 加速度 5 Hz / 陀螺 5 Hz（内部采样率 1 kHz） */
} mpu6050_dlpf_t;

/**
 * @brief 陀螺仪满量程配置枚举。
 */
typedef enum mpu6050_gyro_range_e {
  MPU6050_GYRO_RANGE_250DPS = 0U, /**< ±250 deg/s（灵敏度 131.0 LSB/(deg/s)） */
  MPU6050_GYRO_RANGE_500DPS,      /**< ±500 deg/s（灵敏度 65.5 LSB/(deg/s)） */
  MPU6050_GYRO_RANGE_1000DPS,     /**< ±1000 deg/s（灵敏度 32.8 LSB/(deg/s)） */
  MPU6050_GYRO_RANGE_2000DPS      /**< ±2000 deg/s（灵敏度 16.4 LSB/(deg/s)） */
} mpu6050_gyro_range_t;

/**
 * @brief 加速度计满量程配置枚举。
 */
typedef enum mpu6050_accel_range_e {
  MPU6050_ACCEL_RANGE_2G = 0U, /**< ±2 g（灵敏度 16384.0 LSB/g） */
  MPU6050_ACCEL_RANGE_4G,      /**< ±4 g（灵敏度 8192.0 LSB/g） */
  MPU6050_ACCEL_RANGE_8G,      /**< ±8 g（灵敏度 4096.0 LSB/g） */
  MPU6050_ACCEL_RANGE_16G      /**< ±16 g（灵敏度 2048.0 LSB/g） */
} mpu6050_accel_range_t;

/**
 * @brief MPU6050 运行状态机阶段枚举。
 */
typedef enum mpu6050_state_e {
  MPU6050_STATE_UNINITIALIZED = 0U, /**< 尚未启动或仅完成 WHO_AM_I 探测 */
  MPU6050_STATE_RESET_PENDING,      /**< 待发送软复位命令 */
  MPU6050_STATE_RESET_WAIT,         /**< 软复位后等待 100 ms 稳定窗口 */
  MPU6050_STATE_CALIBRATE_PENDING,  /**< 准备进入静止陀螺零偏采集 */
  MPU6050_STATE_CALIBRATING,        /**< 正在采集静止样本计算零偏与标准差 */
  MPU6050_STATE_READY,              /**< 正常运行与周期采样态 */
  MPU6050_STATE_ERROR               /**< 通信、回读校验或静止校准失败错误态 */
} mpu6050_state_t;

/**
 * @brief MPU6050 初始化配置结构体。
 */
typedef struct imu_mpu6050_cfg_s {
  const char *name;                  /**< 调试名称字符串（非 NULL） */
  bsp_i2c_t i2c;                     /**< I2C 句柄与 7 位从机地址（0x68 或 0x69，timeout_ms > 0） */
  filter_base_t *filter;             /**< 绑定的 9 通道滤波器句柄（非 NULL） */
  mpu6050_dlpf_t dlpf;               /**< 数字低通滤波带宽枚举 */
  mpu6050_gyro_range_t gyro_range;   /**< 陀螺仪满量程枚举 */
  mpu6050_accel_range_t accel_range; /**< 加速度计满量程枚举 */
  uint16_t sample_rate_hz;           /**< 目标采样率（单位：Hz，合法范围 [4, 1000]） */
  uint16_t calibration_samples;      /**< 启动静止零偏校准样本数（0 跳过校准，范围 [0, 1000]） */
  uint8_t axis_source[3];            /**< 车体 [X, Y, Z] 对应的芯片原始轴索引（0..2 的全排列） */
  int8_t axis_sign[3];               /**< 车体 [X, Y, Z] 对应的轴向符号（仅允许 +1 或 -1） */
} imu_mpu6050_cfg_t;

/**
 * @brief 轴映射与单位换算后的未滤波工程单位样本结构体。
 */
typedef struct mpu6050_raw_sample_s {
  float accel_mps2[3]; /**< 车体系三轴加速度（单位：m/s^2） */
  float gyro_dps[3];   /**< 车体系三轴角速度，已扣除零偏（单位：deg/s） */
  float temperature_c; /**< 芯片内部温度（单位：摄氏度 C） */
  uint32_t time_ms;    /**< 采集时的系统单调时间戳（单位：ms） */
} mpu6050_raw_sample_t;

/**
 * @brief MPU6050 静态驱动对象结构体。
 * @note base 必须作为首成员以支持 imu_base_t* 多态转换。
 */
typedef struct imu_mpu6050_s {
  imu_base_t base;                             /**< IMU 多态基类（必须为首成员） */
  imu_mpu6050_cfg_t cfg;                       /**< 初始化配置副本 */
  mpu6050_state_t state;                       /**< 当前驱动状态机阶段 */
  bool is_started;                             /**< 启动标志 */
  uint32_t state_deadline_ms;                  /**< 复位等待截止时间戳（单位：ms） */
  uint32_t next_sample_ms;                     /**< 下一次采样触发时间戳（单位：ms） */
  uint32_t sample_period_ms;                   /**< 采样周期（单位：ms，最小 1 ms） */
  uint8_t sample_rate_divider;                 /**< 写入 SMPLRT_DIV 寄存器的分频值 */
  float accel_lsb_per_g;                       /**< 加速度计灵敏度（单位：LSB/g） */
  float gyro_lsb_per_dps;                      /**< 陀螺仪灵敏度（单位：LSB/(deg/s)） */
  float gyro_bias_dps[3];                      /**< 校准得到的陀螺仪静止零偏（单位：deg/s） */
  uint32_t calibration_count;                  /**< 当前已采集的有效校准样本数 */
  float calibration_gyro_sum[3];               /**< 校准期间陀螺仪各轴累加和（单位：deg/s） */
  float calibration_gyro_sum_sq[3];            /**< 校准期间陀螺仪各轴平方和（单位：(deg/s)^2） */
  mpu6050_raw_sample_t raw_sample;             /**< 最近一次工程单位原始样本（受临界区保护） */
  volatile bool has_raw_sample;                /**< 是否已采集过至少一帧原始样本 */
  volatile bool last_request_failed;           /**< 最近一次 I2C/校准/滤波请求是否失败 */
  volatile uint32_t i2c_error_count;           /**< I2C 读写错误计数 */
  volatile uint32_t configuration_error_count; /**< 寄存器配置与回读校验错误计数 */
  volatile uint32_t calibration_error_count;   /**< 静止校准超差或运动打断错误计数 */
  volatile uint32_t filter_reject_count;       /**< 提交基类滤波管线被拒绝的计数 */
} imu_mpu6050_t;

/**
 * @brief 校验配置并探测 MPU6050 WHO_AM_I 寄存器。
 * @param[out] imu MPU6050 静态对象指针（非 NULL）。
 * @param[in]  cfg 初始化配置结构体指针（非 NULL）。
 * @return 初始化状态码。
 * @retval BSP_OK              配置合法且 WHO_AM_I 校验通过（0x68 或 0x34）。
 * @retval BSP_ERR_INVALID_ARG 参数为空、量程/采样率/轴映射非法。
 * @retval BSP_ERR_HAL         WHO_AM_I 返回值不匹配。
 * @retval 其他 bsp_status_t   底层 BSP_I2C_MemRead 返回的通信错误码。
 * @pre 对应 I2C 外设已完成 CubeMX 初始化。
 */
bsp_status_t Dev_IMU_MPU6050_Init(imu_mpu6050_t *imu, const imu_mpu6050_cfg_t *cfg);

/**
 * @brief 临界区内复制最近一次未滤波的六轴与温度样本。
 * @param[in]  imu        MPU6050 对象指针（非 NULL）。
 * @param[out] out_sample 输出样本结构体指针（非 NULL）。
 * @return 样本是否可用。
 * @retval true  至少已采集过一帧有效原始样本。
 * @retval false 参数为空或尚无有效原始样本。
 */
bool Dev_IMU_MPU6050_GetRawSample(const imu_mpu6050_t *imu, mpu6050_raw_sample_t *out_sample);

/**
 * @brief 请求在 READY 状态重新采集静止陀螺仪零偏。
 * @param[in,out] imu MPU6050 对象指针（非 NULL）。
 * @return 请求状态码。
 * @retval BSP_OK              已切换至 MPU6050_STATE_CALIBRATE_PENDING。
 * @retval BSP_ERR_INVALID_ARG 对象为空、未启动、校准样本数为 0 或当前非 READY 状态。
 */
bsp_status_t Dev_IMU_MPU6050_RequestGyroCalibration(imu_mpu6050_t *imu);

#endif /* HAL_I2C_MODULE_ENABLED || BSP_HOST_STUB */

#endif /* LAYERED_CAR_IMU_MPU6050_H */

/** @} */
