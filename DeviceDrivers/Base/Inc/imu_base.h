//
// Created by Buns_ on 2026/9/1.
//

/**
 * @file imu_base.h
 * @brief IMU 抽象：原始帧提交、滤波调度、滤波结果与运行状态。
 * @addtogroup drivers_base
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_IMU_BASE_H
#define LAYERED_EMBEDDED_C_IMU_BASE_H

#include "filter_base.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct imu_base imu_base_t;

/**
 * @brief IMU 通道位掩码。
 * @note 加速度单位 m/s^2，角速度单位 deg/s，姿态单位 deg。
 */
typedef enum {
  IMU_CHANNEL_YAW     = 1u << 0,
  IMU_CHANNEL_ROLL    = 1u << 1,
  IMU_CHANNEL_PITCH   = 1u << 2,
  IMU_CHANNEL_GYRO_X  = 1u << 3,
  IMU_CHANNEL_GYRO_Y  = 1u << 4,
  IMU_CHANNEL_GYRO_Z  = 1u << 5,
  IMU_CHANNEL_ACCEL_X = 1u << 6,
  IMU_CHANNEL_ACCEL_Y = 1u << 7,
  IMU_CHANNEL_ACCEL_Z = 1u << 8,
} imu_channel_t;

/**
 * @brief IMU 通道数组下标，与通道位号一致。
 */
typedef enum {
  IMU_CHANNEL_INDEX_YAW = 0u,
  IMU_CHANNEL_INDEX_ROLL,
  IMU_CHANNEL_INDEX_PITCH,
  IMU_CHANNEL_INDEX_GYRO_X,
  IMU_CHANNEL_INDEX_GYRO_Y,
  IMU_CHANNEL_INDEX_GYRO_Z,
  IMU_CHANNEL_INDEX_ACCEL_X,
  IMU_CHANNEL_INDEX_ACCEL_Y,
  IMU_CHANNEL_INDEX_ACCEL_Z,
  IMU_CHANNEL_INDEX_COUNT,
} imu_channel_index_t;

#define IMU_CHANNEL_MASK \
  ((uint32_t)((1u << IMU_CHANNEL_INDEX_COUNT) - 1u))

_Static_assert(
  IMU_CHANNEL_INDEX_COUNT <= FILTER_CHANNEL_CAPACITY,
  "IMU channels must fit the filter sample capacity"
);
_Static_assert(
  IMU_CHANNEL_YAW == (1u << IMU_CHANNEL_INDEX_YAW) &&
  IMU_CHANNEL_ROLL == (1u << IMU_CHANNEL_INDEX_ROLL) &&
  IMU_CHANNEL_PITCH == (1u << IMU_CHANNEL_INDEX_PITCH) &&
  IMU_CHANNEL_GYRO_X == (1u << IMU_CHANNEL_INDEX_GYRO_X) &&
  IMU_CHANNEL_GYRO_Y == (1u << IMU_CHANNEL_INDEX_GYRO_Y) &&
  IMU_CHANNEL_GYRO_Z == (1u << IMU_CHANNEL_INDEX_GYRO_Z) &&
  IMU_CHANNEL_ACCEL_X == (1u << IMU_CHANNEL_INDEX_ACCEL_X) &&
  IMU_CHANNEL_ACCEL_Y == (1u << IMU_CHANNEL_INDEX_ACCEL_Y) &&
  IMU_CHANNEL_ACCEL_Z == (1u << IMU_CHANNEL_INDEX_ACCEL_Z),
  "IMU channel masks and indices must match"
);

/**
 * @brief 一次完整 IMU 数据快照。
 * @note 未置位的 valid_mask 通道对应值没有意义。
 * @note channel_update_ms 用于异步通道的新鲜度判断。
 */
typedef struct {
  float    accel_mps2[3];
  float    gyro_dps[3];
  float    yaw_deg;
  float    roll_deg;
  float    pitch_deg;
  uint32_t valid_mask;
  uint32_t channel_update_ms[IMU_CHANNEL_INDEX_COUNT];
  uint32_t sample_time_ms;
  uint32_t sequence;
} imu_sample_t;

/**
 * @brief IMU 运行状态快照。
 * @note valid_mask、时间戳和 sample_count 由 imu_base 统一维护。
 */
typedef struct {
  bool     is_started;
  bool     is_busy;
  bool     has_timestamp;
  bool     last_request_failed;
  uint32_t valid_mask;
  uint32_t sample_count;
  uint32_t error_count;
  uint32_t last_update_ms;
  uint32_t channel_update_ms[IMU_CHANNEL_INDEX_COUNT];
} imu_status_t;

/**
 * @brief IMU 具体实现的操作表。
 * @note process 在任务上下文运行；set_zero 只处理具体设备侧动作。
 */
typedef struct {
  void (*start)      (imu_base_t *this);
  void (*process)    (imu_base_t *this, uint32_t now_ms);
  void (*get_status) (imu_base_t *this, imu_status_t *status);
  void (*set_zero)   (imu_base_t *this);
} imu_ops_t;

/**
 * @brief IMU 抽象对象与统一滤波管线。
 * @note filter 是非 owning 指针，生命周期必须覆盖 IMU 对象。
 * @note filter 的输出通道沿用 imu_channel_t 编号。
 */
typedef struct imu_base {
  const imu_ops_t *ops;
  const char      *name;
  filter_base_t   *filter;

  imu_sample_t raw_sample;
  imu_sample_t sample;
  bool         is_started;
  uint32_t     sample_count;
  float        yaw_offset_deg;
} imu_base_t;

/**
 * @brief 初始化 IMU 抽象层。
 * @param this 对象。
 * @param name 名称。
 * @param ops 具体实现操作表。
 * @param filter 已初始化的静态滤波器根节点，不能为空。
 */
void Dev_IMU_Base_Init(
  imu_base_t *this,
  const char *name,
  const imu_ops_t *ops,
  filter_base_t *filter
);

/** @brief 复位滤波器并启动具体 IMU。 */
void Dev_IMU_Base_Start(imu_base_t *this);

/**
 * @brief 推进 IMU 的非阻塞状态机。
 * @param this IMU 对象。
 * @param now_ms 当前毫秒时间戳。
 * @note 每个任务周期调用一次。
 */
void Dev_IMU_Base_Process(imu_base_t *this, uint32_t now_ms);

/**
 * @brief 提交一帧由具体驱动采集的原始数据。
 * @param this IMU 对象。
 * @param frame 原始滤波样本，只标记本帧更新的通道。
 * @return 滤波器是否产生了至少一个可发布通道。
 * @note 仅允许单写者调用；输入值必须是工程单位。
 * @note 调用方不能保持 frame；base 会完成滤波和结果合并。
 */
bool Dev_IMU_Base_SubmitSample(
  imu_base_t *this,
  const filter_sample_t *frame
);

/** @brief 复制一份经过滤波的 IMU 数据快照。 */
bool Dev_IMU_Base_GetSample(imu_base_t *this, imu_sample_t *sample);

/** @brief 复制一份未经软件滤波的 IMU 数据快照。 */
bool Dev_IMU_Base_GetRawSample(imu_base_t *this, imu_sample_t *sample);

/** @brief 复制一份 IMU 运行状态快照。 */
void Dev_IMU_Base_GetStatus(imu_base_t *this, imu_status_t *status);

/** @brief 读取滤波后的 yaw（度）。 */
float Dev_IMU_Base_GetYaw(imu_base_t *this);
/** @brief 读取滤波后的 roll（度）。 */
float Dev_IMU_Base_GetRoll(imu_base_t *this);
/** @brief 读取滤波后的 pitch（度）。 */
float Dev_IMU_Base_GetPitch(imu_base_t *this);
/** @brief 读取滤波后的 X 轴角速度（deg/s）。 */
float Dev_IMU_Base_GetGyroX(imu_base_t *this);
/** @brief 读取滤波后的 Y 轴角速度（deg/s）。 */
float Dev_IMU_Base_GetGyroY(imu_base_t *this);
/** @brief 读取滤波后的 Z 轴角速度（deg/s）。 */
float Dev_IMU_Base_GetGyroZ(imu_base_t *this);
/** @brief 读取滤波后的 X 轴加速度（m/s^2）。 */
float Dev_IMU_Base_GetAccelX(imu_base_t *this);
/** @brief 读取滤波后的 Y 轴加速度（m/s^2）。 */
float Dev_IMU_Base_GetAccelY(imu_base_t *this);
/** @brief 读取滤波后的 Z 轴加速度（m/s^2）。 */
float Dev_IMU_Base_GetAccelZ(imu_base_t *this);

/**
 * @brief 把当前滤波后的 yaw 设为零参考，不改变 raw_sample。
 * @note 该操作只影响对外发布值，不修改滤波器内部状态。
 */
void Dev_IMU_Base_ZeroYaw(imu_base_t *this);

/** @brief 请求具体 IMU 执行设备侧置零动作。 */
void Dev_IMU_Base_SetZero(imu_base_t *this);

/** @} */

#endif //LAYERED_EMBEDDED_C_IMU_BASE_H
