//
// Created by Buns_ on 2026/9/1.
//

/**
 * @file imu_wht101_usart.h
 * @brief WHT101 UART IMU 具体实现。
 * @addtogroup drivers_concrete
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_IMU_WHT101_USART_H
#define LAYERED_EMBEDDED_C_IMU_WHT101_USART_H

#include "imu_base.h"
#include "usart_base.h"
#include <stdbool.h>

/**
 * @brief 帧解析状态。
 */
typedef enum {
  IMU_WHT101_STATE_SYNCING_1 = 0U,
  IMU_WHT101_STATE_SYNCING_2,
  IMU_WHT101_STATE_RECEIVING_DATA,
  IMU_WHT101_STATE_CALCULATING_SUM,
} imu_wht101_state_t;

/**
 * @brief WHT101 寄存器地址。
 */
typedef enum {
  IMU_WHT101_REG_SAVE    = 0x00,
  IMU_WHT101_REG_RRATE   = 0x03,
  IMU_WHT101_REG_BAUD    = 0x04,
  IMU_WHT101_REG_KEY     = 0x69,
  IMU_WHT101_REG_CALIYAW = 0x76,
  IMU_WHT101_REG_MANCALI = 0xA6,
} imu_wht101_reg_t;

/**
 * @brief 输出速率配置值。
 */
typedef enum {
  IMU_RATE_10HZ  = 0x06,
  IMU_RATE_100HZ = 0x09,
  IMU_RATE_200HZ = 0x0B,
} imu_wht101_rate_t;

/**
 * @brief 非阻塞配置状态机状态。
 */
typedef enum {
  IMU_WHT101_CONFIG_IDLE = 0U,
  IMU_WHT101_CONFIG_ZERO_WAIT_CALIYAW,
  IMU_WHT101_CONFIG_ZERO_WAIT_SAVE,
  IMU_WHT101_CONFIG_RATE_WAIT_WRITE,
  IMU_WHT101_CONFIG_RATE_WAIT_SAVE,
} imu_wht101_config_state_t;

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

#define IMU_WHT101_RX_BUFFER_SIZE 256u
#define IMU_WHT101_RX_INDEX_MASK \
  ((uint16_t)(IMU_WHT101_RX_BUFFER_SIZE - 1u))

/**
 * @brief WHT101 对象。
 * @note rx_buffer 是单字节中断接收的环形队列；解析在 process 中完成。
 * @note 置零和输出速率配置通过 zero_pending / rate_pending 非阻塞推进。
 */
typedef struct {
  imu_base_t imu_base;
  usart_base_t *usart;

  imu_wht101_state_t state;
  uint8_t frame[IMU_WHT101_FRAME_LEN];
  uint8_t frame_idx;

  volatile float yaw;
  volatile float gyro_z;

  bool is_started;
  bool has_yaw;
  bool has_gyro_z;
  bool has_timestamp;

  imu_wht101_config_state_t config_state;
  uint32_t config_deadline_ms;
  volatile uint32_t last_update_ms;
  volatile uint32_t channel_update_ms[IMU_CHANNEL_INDEX_COUNT];
  bool zero_pending;
  bool rate_pending;
  volatile bool config_failed;
  uint8_t rate_pending_value;

  uint8_t rx_byte;
  uint8_t rx_buffer[IMU_WHT101_RX_BUFFER_SIZE];
  volatile uint16_t rx_write;
  volatile uint16_t rx_read;

  volatile uint32_t rx_byte_count;
  volatile uint32_t rx_overflow_count;
  volatile uint32_t rx_error_count;
  volatile uint32_t rx_checksum_error_count;
  volatile uint32_t rx_frame_count;
  volatile bool rx_resync_requested;
} imu_wht101_usart_t;

/**
 * @brief 初始化 WHT101。
 * @param this 对象。
 * @param name 名称。
 * @param usart 已初始化的 usart_base 对象，需支持中断接收。
 */
void Dev_IMU_WHT101_USART_Init          (imu_wht101_usart_t *this, const char *name, usart_base_t *usart);
/**
 * @brief 解析一个字节。
 * @param this 对象。
 * @param byte 输入字节。
 * @param now_ms 当前毫秒时间戳。
 * @note 该函数是解析器，不是 ISR 入队接口；应在任务上下文调用。
 */
void Dev_IMU_WHT101_USART_FeedByte      (imu_wht101_usart_t *this, uint8_t byte, uint32_t now_ms);
/**
 * @brief 请求设置 yaw 零点。
 * @note 非阻塞；实际写入由 process 状态机推进。
 */
void Dev_IMU_WHT101_USART_SetZero       (imu_wht101_usart_t *this);
/**
 * @brief 请求设置输出速率。
 * @param this 对象。
 * @param rate 速率配置值。
 * @note 非阻塞；实际写入由 process 状态机推进。
 */
void Dev_IMU_WHT101_USART_SetOutputRate (imu_wht101_usart_t *this, uint8_t rate);

/** @} */

#endif //LAYERED_EMBEDDED_C_IMU_WHT101_USART_H
