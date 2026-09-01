//
// Created by Buns_ on 2026/9/1.
//

#ifndef LAYERED_EMBEDDED_C_IMU_BASE_H
#define LAYERED_EMBEDDED_C_IMU_BASE_H

typedef struct imu_base imu_base_t;

typedef struct {
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
float Dev_IMU_Base_GetYaw   (imu_base_t *this);
float Dev_IMU_Base_GetRoll  (imu_base_t *this);
float Dev_IMU_Base_GetPitch (imu_base_t *this);
float Dev_IMU_Base_GetGyroX (imu_base_t *this);
float Dev_IMU_Base_GetGyroY (imu_base_t *this);
float Dev_IMU_Base_GetGyroZ (imu_base_t *this);
void  Dev_IMU_Base_SetZero  (imu_base_t *this);

#endif //LAYERED_EMBEDDED_C_IMU_BASE_H
