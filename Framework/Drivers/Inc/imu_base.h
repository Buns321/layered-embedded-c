/**
 * @file imu_base.h
 * @brief 9 通道 IMU 多态基类、统一滤波管线与线程安全快照接口。
 * @addtogroup drivers
 * @{
 */

#ifndef LAYERED_CAR_IMU_BASE_H
#define LAYERED_CAR_IMU_BASE_H

#include "filter_base.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct imu_base_s imu_base_t;

/**
 * @brief IMU 通道位掩码（加速度 m/s^2、陀螺 deg/s、姿态 deg）。
 */
typedef enum imu_channel_e {
  IMU_CHANNEL_YAW = 1U << 0U,
  IMU_CHANNEL_ROLL = 1U << 1U,
  IMU_CHANNEL_PITCH = 1U << 2U,
  IMU_CHANNEL_GYRO_X = 1U << 3U,
  IMU_CHANNEL_GYRO_Y = 1U << 4U,
  IMU_CHANNEL_GYRO_Z = 1U << 5U,
  IMU_CHANNEL_ACCEL_X = 1U << 6U,
  IMU_CHANNEL_ACCEL_Y = 1U << 7U,
  IMU_CHANNEL_ACCEL_Z = 1U << 8U
} imu_channel_t;

/**
 * @brief IMU 通道数组下标。
 */
typedef enum imu_channel_index_e {
  IMU_CHANNEL_INDEX_YAW = 0U,
  IMU_CHANNEL_INDEX_ROLL,
  IMU_CHANNEL_INDEX_PITCH,
  IMU_CHANNEL_INDEX_GYRO_X,
  IMU_CHANNEL_INDEX_GYRO_Y,
  IMU_CHANNEL_INDEX_GYRO_Z,
  IMU_CHANNEL_INDEX_ACCEL_X,
  IMU_CHANNEL_INDEX_ACCEL_Y,
  IMU_CHANNEL_INDEX_ACCEL_Z,
  IMU_CHANNEL_INDEX_COUNT
} imu_channel_index_t;

#define IMU_CHANNEL_MASK ((uint32_t)((1UL << IMU_CHANNEL_INDEX_COUNT) - 1UL))

_Static_assert(IMU_CHANNEL_INDEX_COUNT <= FILTER_CHANNEL_CAPACITY,
               "IMU channels must fit filter sample capacity");

/**
 * @brief IMU 一致性数据快照。
 */
typedef struct imu_sample_s {
  float accel_mps2[3];
  float gyro_dps[3];
  float yaw_deg;
  float roll_deg;
  float pitch_deg;
  uint32_t valid_mask;
  uint32_t channel_update_ms[IMU_CHANNEL_INDEX_COUNT];
  uint32_t sample_time_ms;
  uint32_t sequence;
} imu_sample_t;

/**
 * @brief IMU 运行状态快照。
 */
typedef struct imu_status_s {
  bool is_started;
  bool is_busy;
  bool has_timestamp;
  bool last_request_failed;
  uint32_t valid_mask;
  uint32_t sample_count;
  uint32_t error_count;
  uint32_t last_update_ms;
  uint32_t channel_update_ms[IMU_CHANNEL_INDEX_COUNT];
} imu_status_t;

/**
 * @brief 具体 IMU 设备虚表。
 */
typedef struct imu_ops_s {
  void (*start)(imu_base_t *imu_h);
  void (*process)(imu_base_t *imu_h, uint32_t now_ms);
  void (*get_status)(imu_base_t *imu_h, imu_status_t *status);
  void (*set_zero)(imu_base_t *imu_h);
} imu_ops_t;

/**
 * @brief IMU 多态基类；须作为具体 IMU 结构体首成员。
 */
typedef struct imu_base_s {
  const imu_ops_t *ops;
  const char *name;
  filter_base_t *filter_h;
  imu_sample_t raw_sample;
  imu_sample_t sample;
  bool is_started;
  uint32_t sample_count;
  float yaw_offset_deg;
} imu_base_t;

/** @brief 绑定 IMU 设备虚表与滤波器（仅初始化阶段调用）。 */
void Dev_IMU_Base_Init(imu_base_t *imu_h, const char *name, const imu_ops_t *ops,
                       filter_base_t *filter_h);

/** @brief 复位滤波器并启动具体 IMU（任务上下文调用一次）。 */
void Dev_IMU_Base_Start(imu_base_t *imu_h);

/** @brief 推进 IMU 非阻塞采样与状态机（单一传感器任务周期调用）。 */
void Dev_IMU_Base_Process(imu_base_t *imu_h, uint32_t now_ms);

/** @brief 清洗原始帧、运行滤波并原子合并发布快照。 */
bool Dev_IMU_Base_SubmitSample(imu_base_t *imu_h, const filter_sample_t *frame);

/** @brief 临界区内复制最近一次滤波 IMU 快照；尚无有效样本时返回 false。 */
bool Dev_IMU_Base_GetSample(const imu_base_t *imu_h, imu_sample_t *sample);

/** @brief 临界区内复制最近一次清洗后原始 IMU 快照；尚无有效样本时返回 false。 */
bool Dev_IMU_Base_GetRawSample(const imu_base_t *imu_h, imu_sample_t *sample);

/** @brief 合并具体设备状态与基类管线状态。 */
void Dev_IMU_Base_GetStatus(imu_base_t *imu_h, imu_status_t *status);

/** @brief 读取当前相对零参考的 yaw，单位 deg（无有效样本返回 0）。 */
float Dev_IMU_Base_GetYaw(const imu_base_t *imu_h);

/** @brief 读取滤波后 roll，单位 deg。 */
float Dev_IMU_Base_GetRoll(const imu_base_t *imu_h);

/** @brief 读取滤波后 pitch，单位 deg。 */
float Dev_IMU_Base_GetPitch(const imu_base_t *imu_h);

/** @brief 读取滤波后 X 轴角速度，单位 deg/s。 */
float Dev_IMU_Base_GetGyroX(const imu_base_t *imu_h);

/** @brief 读取滤波后 Y 轴角速度，单位 deg/s。 */
float Dev_IMU_Base_GetGyroY(const imu_base_t *imu_h);

/** @brief 读取滤波后 Z 轴角速度，单位 deg/s。 */
float Dev_IMU_Base_GetGyroZ(const imu_base_t *imu_h);

/** @brief 读取滤波后 X 轴加速度，单位 m/s^2。 */
float Dev_IMU_Base_GetAccelX(const imu_base_t *imu_h);

/** @brief 读取滤波后 Y 轴加速度，单位 m/s^2。 */
float Dev_IMU_Base_GetAccelY(const imu_base_t *imu_h);

/** @brief 读取滤波后 Z 轴加速度，单位 m/s^2。 */
float Dev_IMU_Base_GetAccelZ(const imu_base_t *imu_h);

/** @brief 软件将当前有效 yaw 置为相对零参考。 */
void Dev_IMU_Base_ZeroYaw(imu_base_t *imu_h);

/** @brief 请求具体设备执行硬件侧置零。 */
void Dev_IMU_Base_SetZero(imu_base_t *imu_h);

/** @} */

#endif /* LAYERED_CAR_IMU_BASE_H */
