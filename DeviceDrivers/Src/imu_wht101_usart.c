//
// Created by Buns_ on 2026/9/1.
//

#include "../Inc/imu_wht101_usart.h"
#include "../../System/Inc/system_delay.h"
#include <stdint.h>

static float imu_wht101_get_yaw(imu_base_t *this) {
  imu_wht101_usart_t *self = (imu_wht101_usart_t*)this;
  return self->yaw;
}

static float imu_wht101_get_gyro_z(imu_base_t *this) {
  imu_wht101_usart_t *self = (imu_wht101_usart_t*)this;
  return self->gyro_z;
}

static void imu_wht101_set_zero(imu_base_t *this) {
  imu_wht101_usart_t *self = (imu_wht101_usart_t*)this;
  Dev_IMU_WHT101_USART_SetZero(self);
}

static const imu_ops_t imu_ops_wht101 = {
  .get_yaw    = imu_wht101_get_yaw,
  .get_gyro_z = imu_wht101_get_gyro_z,
  .set_zero   = imu_wht101_set_zero,
};

/******************************************************************************/

static void imu_wht101_write_reg(imu_wht101_usart_t *self, imu_wht101_reg_t reg, uint16_t val) {
  uint8_t cmd[IMU_WHT101_WRITE_LEN] = {
    IMU_WHT101_WRITE_HDR1,
    IMU_WHT101_WRITE_HDR2,
    (uint8_t)reg,
    (uint8_t)(val & 0xFF),
    (uint8_t)((val >> 8) & 0xFF)
  };
  Platform_UART_Transmit(self->uart, cmd, IMU_WHT101_WRITE_LEN, 10);
}

/******************************************************************************/

static void imu_wht101_parse_angle(imu_wht101_usart_t *self) {
  // 角度帧: 55 53 00 00 00 00 YawL YawH VL VH SUM
  int16_t yaw_raw = (int16_t)((self->frame[7] << 8) | self->frame[6]);
  self->yaw = yaw_raw / IMU_WHT101_SCALE * IMU_WHT101_YAW_RANGE;
}

static void imu_wht101_parse_gyro(imu_wht101_usart_t *self) {
  // 角速度帧: 55 52 00 00 RWzL RWzH WzL WzH 00 00 SUM
  int16_t wz_raw = (int16_t)((self->frame[7] << 8) | self->frame[6]);
  self->gyro_z = wz_raw / IMU_WHT101_SCALE * IMU_WHT101_GYRO_RANGE;
}

/******************************************************************************/

void Dev_IMU_WHT101_USART_Init(imu_wht101_usart_t *this, const char *name, Platform_UART_Instance uart) {
  Dev_IMU_Base_Init(&this->imu_base, name, &imu_ops_wht101);
  this->uart      = uart;
  this->state     = IMU_WHT101_STATE_SYNCING_1;
  this->frame_idx = 0;
  this->yaw       = 0;
  this->gyro_z    = 0;
}

void Dev_IMU_WHT101_USART_FeedByte(imu_wht101_usart_t *this, uint8_t byte) {
  switch (this->state) {
    case IMU_WHT101_STATE_SYNCING_1:
      if (byte == IMU_WHT101_FRAME_HEADER) {
        this->frame[0] = byte;
        this->frame_idx = 1;
        this->state = IMU_WHT101_STATE_SYNCING_2;
      }
      break;

    case IMU_WHT101_STATE_SYNCING_2:
      if (byte == IMU_WHT101_TYPE_GYRO || byte == IMU_WHT101_TYPE_ANGLE) {
        this->frame[1] = byte;
        this->frame_idx = 2;
        this->state = IMU_WHT101_STATE_RECEIVING_DATA;
      } else if (byte == IMU_WHT101_FRAME_HEADER) {
        this->frame[0] = byte;
        this->frame_idx = 1;
      } else {
        this->state = IMU_WHT101_STATE_SYNCING_1;
      }
      break;

    case IMU_WHT101_STATE_RECEIVING_DATA:
      this->frame[this->frame_idx++] = byte;
      if (this->frame_idx == IMU_WHT101_FRAME_LEN - 1) {
        this->state = IMU_WHT101_STATE_CALCULATING_SUM;
      }
      break;

    case IMU_WHT101_STATE_CALCULATING_SUM: {
      this->frame[IMU_WHT101_FRAME_LEN - 1] = byte;
      uint8_t sum = 0;
      for (int i = 0; i < IMU_WHT101_FRAME_LEN - 1; i++) sum += this->frame[i];
      if (sum == this->frame[IMU_WHT101_FRAME_LEN - 1]) {
        if      (this->frame[1] == IMU_WHT101_TYPE_GYRO)  imu_wht101_parse_gyro(this);
        else if (this->frame[1] == IMU_WHT101_TYPE_ANGLE) imu_wht101_parse_angle(this);
      }
      this->state = IMU_WHT101_STATE_SYNCING_1;
      break;
    }
  }
}

void Dev_IMU_WHT101_USART_SetZero(imu_wht101_usart_t *this) {
  imu_wht101_write_reg(this, IMU_WHT101_REG_KEY,     IMU_WHT101_KEY_UNLOCK);
  System_Delay_Ms(IMU_WHT101_DELAY_UNLOCK_MS);
  imu_wht101_write_reg(this, IMU_WHT101_REG_CALIYAW, IMU_WHT101_CALIYAW_ZERO);
  System_Delay_Ms(IMU_WHT101_DELAY_SAVE_MS);
  imu_wht101_write_reg(this, IMU_WHT101_REG_SAVE,    IMU_WHT101_SAVE_VALUE);
}

void Dev_IMU_WHT101_USART_SetOutputRate(imu_wht101_usart_t *this, uint8_t rate) {
  imu_wht101_write_reg(this, IMU_WHT101_REG_KEY,   IMU_WHT101_KEY_UNLOCK);
  System_Delay_Ms(IMU_WHT101_DELAY_UNLOCK_MS);
  imu_wht101_write_reg(this, IMU_WHT101_REG_RRATE, rate);
  System_Delay_Ms(IMU_WHT101_DELAY_WRITE_MS);
  imu_wht101_write_reg(this, IMU_WHT101_REG_SAVE,  IMU_WHT101_SAVE_VALUE);
}
