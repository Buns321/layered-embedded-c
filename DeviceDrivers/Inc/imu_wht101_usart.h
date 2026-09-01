//
// Created by Buns_ on 2026/9/1.
//

#ifndef LAYERED_EMBEDDED_C_IMU_WHT101_USART_H
#define LAYERED_EMBEDDED_C_IMU_WHT101_USART_H

#include "../Base/Inc/imu_base.h"
#include "../../Platform/Inc/platform_usart.h"

typedef enum {
  IMU_WHT101_STATE_SYNCING_1 = 0U,
  IMU_WHT101_STATE_SYNCING_2,
  IMU_WHT101_STATE_RECEIVING_DATA,
  IMU_WHT101_STATE_CALCULATING_SUM,
} imu_wht101_state_t;

typedef enum {
  IMU_WHT101_REG_SAVE    = 0x00,
  IMU_WHT101_REG_RRATE   = 0x03,
  IMU_WHT101_REG_BAUD    = 0x04,
  IMU_WHT101_REG_KEY     = 0x69,
  IMU_WHT101_REG_CALIYAW = 0x76,
  IMU_WHT101_REG_MANCALI = 0xA6,
} imu_wht101_reg_t;

typedef enum {
  IMU_RATE_10HZ  = 0x06,
  IMU_RATE_100HZ = 0x09,
  IMU_RATE_200HZ = 0x0B,
} imu_wht101_rate_t;

#define IMU_WHT101_SCALE      32768.0f
#define IMU_WHT101_YAW_RANGE  180.0f
#define IMU_WHT101_GYRO_RANGE 2000.0f

#define IMU_WHT101_FRAME_HEADER    0x55
#define IMU_WHT101_FRAME_LEN       11
#define IMU_WHT101_TYPE_GYRO       0x52
#define IMU_WHT101_TYPE_ANGLE      0x53
#define IMU_WHT101_WRITE_HDR1      0xFF
#define IMU_WHT101_WRITE_HDR2      0xAA
#define IMU_WHT101_KEY_UNLOCK      0xB588
#define IMU_WHT101_DELAY_UNLOCK_MS 200
#define IMU_WHT101_DELAY_SAVE_MS   500
#define IMU_WHT101_DELAY_WRITE_MS  100
#define IMU_WHT101_WRITE_LEN       5
#define IMU_WHT101_SAVE_VALUE      0x0000
#define IMU_WHT101_CALIYAW_ZERO    0x0000

typedef struct {
  imu_base_t imu_base;
  Platform_UART_Instance uart;

  imu_wht101_state_t state;
  uint8_t frame[IMU_WHT101_FRAME_LEN];
  uint8_t frame_idx;

  volatile float yaw;
  volatile float gyro_z;
} imu_wht101_usart_t;

void Dev_IMU_WHT101_USART_Init          (imu_wht101_usart_t *this, const char *name, Platform_UART_Instance uart);
void Dev_IMU_WHT101_USART_FeedByte      (imu_wht101_usart_t *this, uint8_t byte);
void Dev_IMU_WHT101_USART_SetZero       (imu_wht101_usart_t *this);
void Dev_IMU_WHT101_USART_SetOutputRate (imu_wht101_usart_t *this, uint8_t rate);

#endif //LAYERED_EMBEDDED_C_IMU_WHT101_USART_H
