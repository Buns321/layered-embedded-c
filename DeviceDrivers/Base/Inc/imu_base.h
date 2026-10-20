//
// Created by Buns_ on 2026/9/1.
//

#ifndef LAYERED_EMBEDDED_C_IMU_BASE_H
#define LAYERED_EMBEDDED_C_IMU_BASE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct imu_base imu_base_t;

typedef enum {
  IMU_CHANNEL_YAW    = 1u << 0,
  IMU_CHANNEL_ROLL   = 1u << 1,
  IMU_CHANNEL_PITCH  = 1u << 2,
  IMU_CHANNEL_GYRO_X = 1u << 3,
  IMU_CHANNEL_GYRO_Y = 1u << 4,
  IMU_CHANNEL_GYRO_Z = 1u << 5,
} imu_channel_t;

typedef enum {
  IMU_CHANNEL_INDEX_YAW = 0u,
  IMU_CHANNEL_INDEX_ROLL,
  IMU_CHANNEL_INDEX_PITCH,
  IMU_CHANNEL_INDEX_GYRO_X,
  IMU_CHANNEL_INDEX_GYRO_Y,
  IMU_CHANNEL_INDEX_GYRO_Z,
  IMU_CHANNEL_INDEX_COUNT,
} imu_channel_index_t;

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

typedef struct imu_base {
  const imu_ops_t *ops;
  const char      *name;
} imu_base_t;

void  Dev_IMU_Base_Init     (imu_base_t *this, const char *name, const imu_ops_t *ops);
void  Dev_IMU_Base_Start    (imu_base_t *this);
void  Dev_IMU_Base_Process  (imu_base_t *this, uint32_t now_ms);
void  Dev_IMU_Base_GetStatus(imu_base_t *this, imu_status_t *status);
float Dev_IMU_Base_GetYaw   (imu_base_t *this);
float Dev_IMU_Base_GetRoll  (imu_base_t *this);
float Dev_IMU_Base_GetPitch (imu_base_t *this);
float Dev_IMU_Base_GetGyroX (imu_base_t *this);
float Dev_IMU_Base_GetGyroY (imu_base_t *this);
float Dev_IMU_Base_GetGyroZ (imu_base_t *this);
void  Dev_IMU_Base_SetZero  (imu_base_t *this);

#endif //LAYERED_EMBEDDED_C_IMU_BASE_H
