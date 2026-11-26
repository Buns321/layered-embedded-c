/**
 * @file imu_wht101.h
 * @brief WHT101 串口陀螺仪驱动（直连 bsp_uart_t，内置可选上电自动归零状态机）。
 * @addtogroup drivers
 * @{
 */

#ifndef LAYERED_CAR_IMU_WHT101_H
#define LAYERED_CAR_IMU_WHT101_H

#include "bsp_def.h"
#include "bsp_uart.h"
#include "imu_base.h"
#include <stdbool.h>
#include <stdint.h>

#define IMU_WHT101_FRAME_LEN 11U
#define IMU_WHT101_RX_RING_SIZE 256U
#define IMU_WHT101_RX_DMA_SIZE 64U

/**
 * @brief WHT101 输出速率寄存器码。
 */
typedef enum imu_wht101_rate_e {
  IMU_WHT101_RATE_10HZ = 0x06U,
  IMU_WHT101_RATE_100HZ = 0x09U,
  IMU_WHT101_RATE_200HZ = 0x0BU
} imu_wht101_rate_t;

/**
 * @brief 上电自动归零状态机阶段。
 */
typedef enum imu_wht101_boot_state_e {
  IMU_WHT101_BOOT_IDLE = 0U,
  IMU_WHT101_BOOT_RATE_CONFIG,
  IMU_WHT101_BOOT_WAIT_STILL,
  IMU_WHT101_BOOT_ZERO_CONFIG,
  IMU_WHT101_BOOT_WAIT_ZERO_FRAME,
  IMU_WHT101_BOOT_READY,
  IMU_WHT101_BOOT_FAULT
} imu_wht101_boot_state_t;

/**
 * @brief WHT101 初始化配置（0 值超时/阈值字段自动使用安全默认参数）。
 */
typedef struct imu_wht101_cfg_s {
  const char *name;
  UART_HandleTypeDef *huart;
  bsp_uart_rx_mode_t rx_mode; /* NONE 时默认按 BSP_UART_RX_IT_BYTE 初始化 */
  filter_base_t *filter;
  bool auto_zero_on_boot;
  imu_wht101_rate_t boot_rate; /* 0 默认 IMU_WHT101_RATE_10HZ */
  uint32_t cfg_timeout_ms;     /* 默认 3000 ms */
  uint32_t still_timeout_ms;   /* 默认 5000 ms */
  uint32_t still_confirm_ms;   /* 默认 400 ms */
  float still_gyro_dps;        /* 默认 3.0 deg/s */
  uint32_t zero_timeout_ms;    /* 默认 2000 ms */
  float zero_residual_deg;     /* 默认 5.0 deg */
} imu_wht101_cfg_t;

typedef enum imu_wht101_rx_state_e {
  IMU_WHT101_RX_SYNC_1 = 0U,
  IMU_WHT101_RX_SYNC_2,
  IMU_WHT101_RX_DATA,
  IMU_WHT101_RX_CHECKSUM
} imu_wht101_rx_state_t;

typedef enum imu_wht101_cfg_state_e {
  IMU_WHT101_CFG_IDLE = 0U,
  IMU_WHT101_CFG_ZERO_WAIT_CALIYAW,
  IMU_WHT101_CFG_ZERO_WAIT_SAVE,
  IMU_WHT101_CFG_RATE_WAIT_WRITE,
  IMU_WHT101_CFG_RATE_WAIT_SAVE
} imu_wht101_cfg_state_t;

/**
 * @brief WHT101 静态驱动对象（base 必须为首成员，禁止按值拷贝）。
 */
typedef struct imu_wht101_s {
  imu_base_t base;
  bsp_uart_t uart;
  uint8_t rx_ring_buf[IMU_WHT101_RX_RING_SIZE];
  uint8_t rx_dma_buf[IMU_WHT101_RX_DMA_SIZE];
  imu_wht101_rx_state_t rx_state;
  uint8_t frame[IMU_WHT101_FRAME_LEN];
  uint8_t frame_idx;
  bool is_started;
  imu_wht101_cfg_state_t cfg_state;
  uint32_t cfg_deadline_ms;
  bool zero_pending;
  bool rate_pending;
  uint8_t rate_pending_value;
  volatile bool config_failed;
  bool auto_zero_on_boot;
  bool boot_started_pending;
  imu_wht101_rate_t boot_rate;
  imu_wht101_boot_state_t boot_state;
  uint32_t boot_state_started_ms;
  uint32_t config_done_ms;
  uint32_t still_since_ms;
  bool still_tracking;
  uint32_t zero_done_ms;
  uint32_t cfg_timeout_ms;
  uint32_t still_timeout_ms;
  uint32_t still_confirm_ms;
  float still_gyro_dps;
  uint32_t zero_timeout_ms;
  float zero_residual_deg;
  volatile uint32_t tx_error_count;
  volatile uint32_t rx_checksum_error_count;
} imu_wht101_t;

/**
 * @brief 初始化 WHT101 与内部静态 UART 接收通道。
 */
bsp_status_t Dev_IMU_WHT101_Init(imu_wht101_t *imu, const imu_wht101_cfg_t *cfg);

/**
 * @brief 向帧解析状态机喂入单字节（仅任务上下文、已 Start 后调用）。
 */
void Dev_IMU_WHT101_FeedByte(imu_wht101_t *imu, uint8_t byte, uint32_t now_ms);

/**
 * @brief 发起硬件 yaw 归零命令序列（非阻塞，由后续 process 推进）。
 */
void Dev_IMU_WHT101_SetZero(imu_wht101_t *imu);

/**
 * @brief 发起输出速率配置命令序列（非阻塞，由后续 process 推进）。
 */
void Dev_IMU_WHT101_SetOutputRate(imu_wht101_t *imu, uint8_t rate);

/** @brief 查询上电自动归零状态机当前阶段。 */
imu_wht101_boot_state_t Dev_IMU_WHT101_GetBootState(const imu_wht101_t *imu);

/** @brief 查询 IMU 是否已启动且完成初始化（READY）。 */
bool Dev_IMU_WHT101_IsReady(const imu_wht101_t *imu);

/** @brief 查询上电归零状态机是否进入故障状态。 */
bool Dev_IMU_WHT101_HasFault(const imu_wht101_t *imu);

/** @} */

#endif /* LAYERED_CAR_IMU_WHT101_H */
