/**
 * @file imu_wht101.c
 * @brief WHT101 串口陀螺仪驱动实现。
 * @ingroup drivers
 */

#include "imu_wht101.h"
#include "sys_math.h"
#include "sys_time_window.h"
#include <stddef.h>
#include <string.h>

#define WHT101_SCALE 32768.0f
#define WHT101_YAW_RANGE_DEG 180.0f
#define WHT101_GYRO_RANGE_DPS 2000.0f

#define WHT101_FRAME_HEADER 0x55U
#define WHT101_TYPE_GYRO 0x52U
#define WHT101_TYPE_ANGLE 0x53U

#define WHT101_WRITE_HDR1 0xFFU
#define WHT101_WRITE_HDR2 0xAAU
#define WHT101_WRITE_LEN 5U

#define WHT101_REG_SAVE 0x00U
#define WHT101_REG_RRATE 0x03U
#define WHT101_REG_KEY 0x69U
#define WHT101_REG_CALIYAW 0x76U

#define WHT101_KEY_UNLOCK 0xB588U
#define WHT101_SAVE_VALUE 0x0000U
#define WHT101_CALIYAW_ZERO 0x0000U

#define WHT101_DELAY_UNLOCK_MS 200U
#define WHT101_DELAY_SAVE_MS 500U
#define WHT101_DELAY_WRITE_MS 100U
#define WHT101_TX_TIMEOUT_MS 10U

#define WHT101_DEFAULT_CFG_TIMEOUT_MS 3000U
#define WHT101_DEFAULT_STILL_TIMEOUT_MS 5000U
#define WHT101_DEFAULT_STILL_CONFIRM_MS 400U
#define WHT101_DEFAULT_STILL_GYRO_DPS 3.0f
#define WHT101_DEFAULT_ZERO_TIMEOUT_MS 2000U
#define WHT101_DEFAULT_ZERO_RESIDUAL_DEG 5.0f

static bool wht101_write_reg(imu_wht101_t *imu, uint8_t reg, uint16_t value) {
  uint8_t cmd[WHT101_WRITE_LEN] = {
      WHT101_WRITE_HDR1,
      WHT101_WRITE_HDR2,
      reg,
      (uint8_t)(value & 0xFFU),
      (uint8_t)((value >> 8U) & 0xFFU),
  };
  bsp_status_t st = BSP_UART_SendBlocking(&imu->uart, cmd, WHT101_WRITE_LEN, WHT101_TX_TIMEOUT_MS);
  if (st != BSP_OK) {
    imu->tx_error_count++;
    imu->config_failed = true;
    return false;
  }
  return true;
}

static void wht101_submit_channel(imu_wht101_t *imu, imu_channel_index_t channel, float value,
                                  uint32_t now_ms) {
  filter_sample_t sample = {
      .valid_mask = (uint32_t)1U << (uint8_t)channel,
      .time_ms = now_ms,
  };
  sample.value[channel] = value;
  (void)Dev_IMU_Base_SubmitSample(&imu->base, &sample);
}

static void wht101_cfg_start_next(imu_wht101_t *imu, uint32_t now_ms) {
  if (imu->cfg_state != IMU_WHT101_CFG_IDLE) {
    return;
  }

  if (imu->zero_pending) {
    imu->zero_pending = false;
    if (!wht101_write_reg(imu, WHT101_REG_KEY, WHT101_KEY_UNLOCK)) {
      return;
    }
    imu->cfg_state = IMU_WHT101_CFG_ZERO_WAIT_CALIYAW;
    imu->cfg_deadline_ms = now_ms + WHT101_DELAY_UNLOCK_MS;
    return;
  }

  if (imu->rate_pending) {
    imu->rate_pending = false;
    if (!wht101_write_reg(imu, WHT101_REG_KEY, WHT101_KEY_UNLOCK)) {
      return;
    }
    imu->cfg_state = IMU_WHT101_CFG_RATE_WAIT_WRITE;
    imu->cfg_deadline_ms = now_ms + WHT101_DELAY_UNLOCK_MS;
  }
}

static void wht101_cfg_process(imu_wht101_t *imu, uint32_t now_ms) {
  if (imu->cfg_state == IMU_WHT101_CFG_IDLE ||
      !Sys_Time_HasReachedMs(now_ms, imu->cfg_deadline_ms)) {
    wht101_cfg_start_next(imu, now_ms);
    return;
  }

  switch (imu->cfg_state) {
  case IMU_WHT101_CFG_ZERO_WAIT_CALIYAW:
    if (!wht101_write_reg(imu, WHT101_REG_CALIYAW, WHT101_CALIYAW_ZERO)) {
      imu->cfg_state = IMU_WHT101_CFG_IDLE;
      return;
    }
    imu->cfg_state = IMU_WHT101_CFG_ZERO_WAIT_SAVE;
    imu->cfg_deadline_ms = now_ms + WHT101_DELAY_SAVE_MS;
    break;

  case IMU_WHT101_CFG_ZERO_WAIT_SAVE:
    if (!wht101_write_reg(imu, WHT101_REG_SAVE, WHT101_SAVE_VALUE)) {
      imu->cfg_state = IMU_WHT101_CFG_IDLE;
      return;
    }
    imu->cfg_state = IMU_WHT101_CFG_IDLE;
    imu->config_failed = false;
    break;

  case IMU_WHT101_CFG_RATE_WAIT_WRITE:
    if (!wht101_write_reg(imu, WHT101_REG_RRATE, imu->rate_pending_value)) {
      imu->cfg_state = IMU_WHT101_CFG_IDLE;
      return;
    }
    imu->cfg_state = IMU_WHT101_CFG_RATE_WAIT_SAVE;
    imu->cfg_deadline_ms = now_ms + WHT101_DELAY_WRITE_MS;
    break;

  case IMU_WHT101_CFG_RATE_WAIT_SAVE:
    if (!wht101_write_reg(imu, WHT101_REG_SAVE, WHT101_SAVE_VALUE)) {
      imu->cfg_state = IMU_WHT101_CFG_IDLE;
      return;
    }
    imu->cfg_state = IMU_WHT101_CFG_IDLE;
    imu->config_failed = false;
    break;

  case IMU_WHT101_CFG_IDLE:
  default:
    imu->cfg_state = IMU_WHT101_CFG_IDLE;
    break;
  }
}

static void wht101_enter_boot_state(imu_wht101_t *imu, imu_wht101_boot_state_t next,
                                    uint32_t now_ms) {
  imu->boot_state = next;
  imu->boot_state_started_ms = now_ms;
  imu->still_since_ms = 0U;
  imu->still_tracking = false;
}

static void wht101_boot_process(imu_wht101_t *imu, uint32_t now_ms) {
  if (!imu->auto_zero_on_boot || imu->boot_state == IMU_WHT101_BOOT_READY ||
      imu->boot_state == IMU_WHT101_BOOT_FAULT || imu->boot_state == IMU_WHT101_BOOT_IDLE) {
    return;
  }

  imu_status_t status = {0};
  Dev_IMU_Base_GetStatus(&imu->base, &status);

  switch (imu->boot_state) {
  case IMU_WHT101_BOOT_RATE_CONFIG:
    if (status.last_request_failed) {
      imu->boot_state = IMU_WHT101_BOOT_FAULT;
      return;
    }
    if (!status.is_busy) {
      imu->config_done_ms = now_ms;
      wht101_enter_boot_state(imu, IMU_WHT101_BOOT_WAIT_STILL, now_ms);
    } else if (Sys_Time_HasElapsedMs(now_ms, imu->boot_state_started_ms, imu->cfg_timeout_ms)) {
      imu->boot_state = IMU_WHT101_BOOT_FAULT;
    }
    break;

  case IMU_WHT101_BOOT_WAIT_STILL: {
    imu_sample_t sample = {0};
    if (Dev_IMU_Base_GetSample(&imu->base, &sample)) {
      bool fresh = ((sample.valid_mask & IMU_CHANNEL_YAW) != 0U) &&
                   ((sample.valid_mask & IMU_CHANNEL_GYRO_Z) != 0U) &&
                   Sys_Time_IsAfterMs(sample.channel_update_ms[IMU_CHANNEL_INDEX_YAW],
                                      imu->config_done_ms) &&
                   Sys_Time_IsAfterMs(sample.channel_update_ms[IMU_CHANNEL_INDEX_GYRO_Z],
                                      imu->config_done_ms);
      bool still = fresh && (SYS_ABS(sample.gyro_dps[2]) <= imu->still_gyro_dps);

      if (still) {
        if (!imu->still_tracking) {
          imu->still_since_ms = now_ms;
          imu->still_tracking = true;
        } else if (Sys_Time_HasElapsedMs(now_ms, imu->still_since_ms, imu->still_confirm_ms)) {
          Dev_IMU_WHT101_SetZero(imu);
          wht101_enter_boot_state(imu, IMU_WHT101_BOOT_ZERO_CONFIG, now_ms);
          return;
        }
      } else {
        imu->still_tracking = false;
      }
    }

    if (Sys_Time_HasElapsedMs(now_ms, imu->boot_state_started_ms, imu->still_timeout_ms)) {
      imu->boot_state = IMU_WHT101_BOOT_FAULT;
    }
    break;
  }

  case IMU_WHT101_BOOT_ZERO_CONFIG:
    if (status.last_request_failed) {
      imu->boot_state = IMU_WHT101_BOOT_FAULT;
      return;
    }
    if (!status.is_busy) {
      imu->zero_done_ms = now_ms;
      wht101_enter_boot_state(imu, IMU_WHT101_BOOT_WAIT_ZERO_FRAME, now_ms);
    } else if (Sys_Time_HasElapsedMs(now_ms, imu->boot_state_started_ms, imu->cfg_timeout_ms)) {
      imu->boot_state = IMU_WHT101_BOOT_FAULT;
    }
    break;

  case IMU_WHT101_BOOT_WAIT_ZERO_FRAME: {
    imu_sample_t sample = {0};
    if (Dev_IMU_Base_GetSample(&imu->base, &sample) &&
        ((sample.valid_mask & IMU_CHANNEL_YAW) != 0U) &&
        Sys_Time_IsAfterMs(sample.channel_update_ms[IMU_CHANNEL_INDEX_YAW], imu->zero_done_ms) &&
        (SYS_ABS(sample.yaw_deg) <= imu->zero_residual_deg)) {
      Dev_IMU_Base_ZeroYaw(&imu->base);
      wht101_enter_boot_state(imu, IMU_WHT101_BOOT_READY, now_ms);
    } else if (Sys_Time_HasElapsedMs(now_ms, imu->boot_state_started_ms, imu->zero_timeout_ms)) {
      imu->boot_state = IMU_WHT101_BOOT_FAULT;
    }
    break;
  }

  case IMU_WHT101_BOOT_IDLE:
  case IMU_WHT101_BOOT_READY:
  case IMU_WHT101_BOOT_FAULT:
  default:
    break;
  }
}

static void wht101_start_op(imu_base_t *imu_h) {
  imu_wht101_t *imu = (imu_wht101_t *)imu_h;
  if (imu == NULL || imu->is_started) {
    return;
  }
  (void)BSP_UART_StartRx(&imu->uart);
  imu->is_started = true;
  if (imu->auto_zero_on_boot) {
    imu->boot_started_pending = true;
    imu->boot_state = IMU_WHT101_BOOT_RATE_CONFIG;
    imu->rate_pending_value = (uint8_t)imu->boot_rate;
    imu->rate_pending = true;
    imu->config_failed = false;
  } else {
    imu->boot_started_pending = false;
    imu->boot_state = IMU_WHT101_BOOT_READY;
  }
}

static void wht101_process_op(imu_base_t *imu_h, uint32_t now_ms) {
  imu_wht101_t *imu = (imu_wht101_t *)imu_h;
  if (imu == NULL || !imu->is_started) {
    return;
  }

  if (imu->boot_started_pending) {
    imu->boot_started_pending = false;
    imu->boot_state_started_ms = now_ms;
  }

  BSP_UART_EnsureRxArmed(&imu->uart);

  uint8_t byte = 0U;
  while (BSP_UART_PopByte(&imu->uart, &byte)) {
    if (BSP_UART_TakeResync(&imu->uart)) {
      imu->rx_state = IMU_WHT101_RX_SYNC_1;
      imu->frame_idx = 0U;
    }
    Dev_IMU_WHT101_FeedByte(imu, byte, now_ms);
  }

  if (BSP_UART_TakeResync(&imu->uart)) {
    imu->rx_state = IMU_WHT101_RX_SYNC_1;
    imu->frame_idx = 0U;
  }

  wht101_cfg_process(imu, now_ms);
  wht101_boot_process(imu, now_ms);
}

static void wht101_get_status_op(imu_base_t *imu_h, imu_status_t *status) {
  imu_wht101_t *imu = (imu_wht101_t *)imu_h;
  if (imu == NULL || status == NULL) {
    return;
  }

  uint64_t err_sum = (uint64_t)imu->tx_error_count + (uint64_t)imu->uart.rx_error_count +
                     (uint64_t)imu->uart.rx_dropped_bytes + (uint64_t)imu->rx_checksum_error_count;
  if (err_sum > UINT32_MAX) {
    err_sum = UINT32_MAX;
  }

  *status = (imu_status_t){
      .is_started = imu->is_started,
      .is_busy = (imu->cfg_state != IMU_WHT101_CFG_IDLE) || imu->zero_pending || imu->rate_pending,
      .last_request_failed = imu->config_failed,
      .error_count = (uint32_t)err_sum,
  };
}

static void wht101_set_zero_op(imu_base_t *imu_h) { Dev_IMU_WHT101_SetZero((imu_wht101_t *)imu_h); }

static const imu_ops_t s_imu_ops_wht101 = {
    .start = wht101_start_op,
    .process = wht101_process_op,
    .get_status = wht101_get_status_op,
    .set_zero = wht101_set_zero_op,
};

bsp_status_t Dev_IMU_WHT101_Init(imu_wht101_t *imu, const imu_wht101_cfg_t *cfg) {
  if (imu == NULL || cfg == NULL || cfg->name == NULL || cfg->huart == NULL ||
      cfg->filter == NULL) {
    return BSP_ERR_INVALID_ARG;
  }

  bsp_uart_rx_mode_t rx_mode =
      (cfg->rx_mode == BSP_UART_RX_NONE) ? BSP_UART_RX_IT_BYTE : cfg->rx_mode;
  if (rx_mode != BSP_UART_RX_IT_BYTE && rx_mode != BSP_UART_RX_DMA_IDLE) {
    return BSP_ERR_INVALID_ARG;
  }

  float still_gyro_dps =
      (cfg->still_gyro_dps > 0.0f) ? cfg->still_gyro_dps : WHT101_DEFAULT_STILL_GYRO_DPS;
  float zero_residual_deg =
      (cfg->zero_residual_deg > 0.0f) ? cfg->zero_residual_deg : WHT101_DEFAULT_ZERO_RESIDUAL_DEG;
  if (!SYS_IS_POS_FINITE(still_gyro_dps) || !SYS_IS_POS_FINITE(zero_residual_deg)) {
    return BSP_ERR_INVALID_ARG;
  }

  (void)memset(imu, 0, sizeof(*imu));

  const bsp_uart_cfg_t uart_cfg = {
      .huart = cfg->huart,
      .rx_mode = rx_mode,
      .rx_ring_buf = imu->rx_ring_buf,
      .rx_ring_size = (uint16_t)sizeof(imu->rx_ring_buf),
      .rx_dma_buf = (rx_mode == BSP_UART_RX_DMA_IDLE) ? imu->rx_dma_buf : NULL,
      .rx_dma_size = (rx_mode == BSP_UART_RX_DMA_IDLE) ? (uint16_t)sizeof(imu->rx_dma_buf) : 0U,
      .on_tx_done = NULL,
      .tx_ctx = NULL,
  };

  bsp_status_t st = BSP_UART_Init(&imu->uart, &uart_cfg);
  if (st != BSP_OK) {
    return st;
  }

  Dev_IMU_Base_Init(&imu->base, cfg->name, &s_imu_ops_wht101, cfg->filter);
  imu->auto_zero_on_boot = cfg->auto_zero_on_boot;
  imu->boot_rate = ((uint32_t)cfg->boot_rate == 0U) ? IMU_WHT101_RATE_10HZ : cfg->boot_rate;
  imu->boot_state = IMU_WHT101_BOOT_IDLE;
  imu->cfg_timeout_ms =
      (cfg->cfg_timeout_ms > 0U) ? cfg->cfg_timeout_ms : WHT101_DEFAULT_CFG_TIMEOUT_MS;
  imu->still_timeout_ms =
      (cfg->still_timeout_ms > 0U) ? cfg->still_timeout_ms : WHT101_DEFAULT_STILL_TIMEOUT_MS;
  imu->still_confirm_ms =
      (cfg->still_confirm_ms > 0U) ? cfg->still_confirm_ms : WHT101_DEFAULT_STILL_CONFIRM_MS;
  imu->still_gyro_dps = still_gyro_dps;
  imu->zero_timeout_ms =
      (cfg->zero_timeout_ms > 0U) ? cfg->zero_timeout_ms : WHT101_DEFAULT_ZERO_TIMEOUT_MS;
  imu->zero_residual_deg = zero_residual_deg;
  return BSP_OK;
}

void Dev_IMU_WHT101_FeedByte(imu_wht101_t *imu, uint8_t byte, uint32_t now_ms) {
  if (imu == NULL) {
    return;
  }

  switch (imu->rx_state) {
  case IMU_WHT101_RX_SYNC_1:
    if (byte == WHT101_FRAME_HEADER) {
      imu->frame[0] = byte;
      imu->frame_idx = 1U;
      imu->rx_state = IMU_WHT101_RX_SYNC_2;
    }
    break;

  case IMU_WHT101_RX_SYNC_2:
    if (byte == WHT101_TYPE_GYRO || byte == WHT101_TYPE_ANGLE) {
      imu->frame[1] = byte;
      imu->frame_idx = 2U;
      imu->rx_state = IMU_WHT101_RX_DATA;
    } else if (byte == WHT101_FRAME_HEADER) {
      imu->frame[0] = byte;
      imu->frame_idx = 1U;
    } else {
      imu->rx_state = IMU_WHT101_RX_SYNC_1;
    }
    break;

  case IMU_WHT101_RX_DATA:
    imu->frame[imu->frame_idx++] = byte;
    if (imu->frame_idx == (IMU_WHT101_FRAME_LEN - 1U)) {
      imu->rx_state = IMU_WHT101_RX_CHECKSUM;
    }
    break;

  case IMU_WHT101_RX_CHECKSUM: {
    imu->frame[IMU_WHT101_FRAME_LEN - 1U] = byte;
    uint8_t sum = 0U;
    for (uint8_t i = 0U; i < (IMU_WHT101_FRAME_LEN - 1U); ++i) {
      sum = (uint8_t)(sum + imu->frame[i]);
    }
    if (sum == byte) {
      int16_t raw = (int16_t)(((uint16_t)imu->frame[7] << 8U) | (uint16_t)imu->frame[6]);
      if (imu->frame[1] == WHT101_TYPE_GYRO) {
        float wz_dps = ((float)raw / WHT101_SCALE) * WHT101_GYRO_RANGE_DPS;
        wht101_submit_channel(imu, IMU_CHANNEL_INDEX_GYRO_Z, wz_dps, now_ms);
      } else if (imu->frame[1] == WHT101_TYPE_ANGLE) {
        float yaw_deg = ((float)raw / WHT101_SCALE) * WHT101_YAW_RANGE_DEG;
        wht101_submit_channel(imu, IMU_CHANNEL_INDEX_YAW, yaw_deg, now_ms);
      }
    } else {
      imu->rx_checksum_error_count++;
    }
    imu->rx_state = IMU_WHT101_RX_SYNC_1;
    break;
  }

  default:
    imu->rx_state = IMU_WHT101_RX_SYNC_1;
    break;
  }
}

void Dev_IMU_WHT101_SetZero(imu_wht101_t *imu) {
  if (imu == NULL || !imu->is_started) {
    return;
  }
  imu->config_failed = false;
  imu->zero_pending = true;
}

void Dev_IMU_WHT101_SetOutputRate(imu_wht101_t *imu, uint8_t rate) {
  if (imu == NULL || !imu->is_started) {
    return;
  }
  imu->config_failed = false;
  imu->rate_pending_value = rate;
  imu->rate_pending = true;
}

imu_wht101_boot_state_t Dev_IMU_WHT101_GetBootState(const imu_wht101_t *imu) {
  return (imu != NULL) ? imu->boot_state : IMU_WHT101_BOOT_FAULT;
}

bool Dev_IMU_WHT101_IsReady(const imu_wht101_t *imu) {
  return (imu != NULL) && imu->is_started && (imu->boot_state == IMU_WHT101_BOOT_READY);
}

bool Dev_IMU_WHT101_HasFault(const imu_wht101_t *imu) {
  return (imu != NULL) && (imu->boot_state == IMU_WHT101_BOOT_FAULT);
}
