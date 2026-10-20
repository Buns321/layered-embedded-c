//
// Created by Buns_ on 2026/9/1.
//

/**
 * @file imu_base.h
 * @brief IMU 抽象：姿态、角速度、状态与异步处理。
 * @addtogroup drivers_base
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_IMU_BASE_H
#define LAYERED_EMBEDDED_C_IMU_BASE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct imu_base imu_base_t;

/**
 * @brief IMU 通道位掩码。
 */
typedef enum {
  IMU_CHANNEL_YAW    = 1u << 0,
  IMU_CHANNEL_ROLL   = 1u << 1,
  IMU_CHANNEL_PITCH  = 1u << 2,
  IMU_CHANNEL_GYRO_X = 1u << 3,
  IMU_CHANNEL_GYRO_Y = 1u << 4,
  IMU_CHANNEL_GYRO_Z = 1u << 5,
} imu_channel_t;

/**
 * @brief IMU 通道数组下标。
 */
typedef enum {
  IMU_CHANNEL_INDEX_YAW = 0u,
  IMU_CHANNEL_INDEX_ROLL,
  IMU_CHANNEL_INDEX_PITCH,
  IMU_CHANNEL_INDEX_GYRO_X,
  IMU_CHANNEL_INDEX_GYRO_Y,
  IMU_CHANNEL_INDEX_GYRO_Z,
  IMU_CHANNEL_INDEX_COUNT,
} imu_channel_index_t;

/**
 * @brief IMU 运行状态快照。
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
 * @note 具体 IMU 不支持的通道可以把对应函数指针留空；base 调用时会断言。
 */
typedef struct {
  void  (*start)      (imu_base_t *this);
  void  (*process)    (imu_base_t *this, uint32_t now_ms);
  void  (*get_status) (imu_base_t *this, imu_status_t *status);
  float (*get_yaw)    (imu_base_t *this);
  float (*get_roll)   (imu_base_t *this);
  float (*get_pitch)  (imu_base_t *this);
  float (*get_gyro_x) (imu_base_t *this);
  float (*get_gyro_y) (imu_base_t *this);
  float (*get_gyro_z) (imu_base_t *this);
  void  (*set_zero)   (imu_base_t *this);
} imu_ops_t;

/**
 * @brief IMU 抽象对象。
 */
typedef struct imu_base {
  const imu_ops_t *ops;
  const char      *name;
} imu_base_t;

/** @brief 初始化 IMU 抽象层并绑定操作表。 */
void  Dev_IMU_Base_Init     (imu_base_t *this, const char *name, const imu_ops_t *ops);
/** @brief 启动 IMU 接收。 */
void  Dev_IMU_Base_Start    (imu_base_t *this);
/**
 * @brief 推进 IMU 的非阻塞状态机。
 * @param this IMU 对象。
 * @param now_ms 当前毫秒时间戳。
 * @note 每个任务周期调用一次。
 */
void  Dev_IMU_Base_Process  (imu_base_t *this, uint32_t now_ms);
/** @brief 复制一份 IMU 状态快照。 */
void  Dev_IMU_Base_GetStatus(imu_base_t *this, imu_status_t *status);
/** @brief 读取 yaw（度）。 */
float Dev_IMU_Base_GetYaw   (imu_base_t *this);
/** @brief 读取 roll（度）。 */
float Dev_IMU_Base_GetRoll  (imu_base_t *this);
/** @brief 读取 pitch（度）。 */
float Dev_IMU_Base_GetPitch (imu_base_t *this);
/** @brief 读取 X 轴角速度。 */
float Dev_IMU_Base_GetGyroX (imu_base_t *this);
/** @brief 读取 Y 轴角速度。 */
float Dev_IMU_Base_GetGyroY (imu_base_t *this);
/** @brief 读取 Z 轴角速度。 */
float Dev_IMU_Base_GetGyroZ (imu_base_t *this);
/** @brief 请求设置零点。 */
void  Dev_IMU_Base_SetZero  (imu_base_t *this);

/** @} */

#endif //LAYERED_EMBEDDED_C_IMU_BASE_H
