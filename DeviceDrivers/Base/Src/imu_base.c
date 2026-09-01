//
// Created by Buns_ on 2026/9/1.
//

#include "../Inc/imu_base.h"
#include <assert.h>

void Dev_IMU_Base_Init(imu_base_t *this, const char *name, const imu_ops_t *ops) {
  assert(this && name &&ops);
  this->name = name;
  this->ops = ops;
}

float Dev_IMU_Base_GetYaw(imu_base_t *this) {
  assert(this && this->ops && this->ops->get_yaw);
  return this->ops->get_yaw(this);
}

float Dev_IMU_Base_GetRoll(imu_base_t *this) {
  assert(this && this->ops && this->ops->get_roll);
  return this->ops->get_roll(this);
}

float Dev_IMU_Base_GetPitch(imu_base_t *this) {
  assert(this && this->ops && this->ops->get_pitch);
  return this->ops->get_pitch(this);
}

float Dev_IMU_Base_GetGyroX(imu_base_t *this) {
  assert(this && this->ops && this->ops->get_gyro_x);
  return this->ops->get_gyro_x(this);
}

float Dev_IMU_Base_GetGyroY(imu_base_t *this) {
  assert(this && this->ops && this->ops->get_gyro_y);
  return this->ops->get_gyro_y(this);
}

float Dev_IMU_Base_GetGyroZ(imu_base_t *this) {
  assert(this && this->ops && this->ops->get_gyro_z);
  return this->ops->get_gyro_z(this);
}

void  Dev_IMU_Base_SetZero(imu_base_t *this) {
  assert(this && this->ops && this->ops->set_zero);
  this->ops->set_zero(this);
}
