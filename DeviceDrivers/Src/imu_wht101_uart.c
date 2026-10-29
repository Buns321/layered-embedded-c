//
// 由 Buns_ 创建于 2026/9/1。
//

/**
 * @file imu_wht101_uart.c
 * @brief WHT101 UART IMU 驱动实现。
 * @ingroup drivers_concrete
 */

#include "imu_wht101_uart.h"
#include <assert.h>
#include <stdint.h>

/**
 * @brief 启动 WHT101 接收与配置。
 */
static void imu_wht101_start(imu_base_t *imu_h);
/**
 * @brief 推进 WHT101 周期处理。
 */
static void imu_wht101_process(imu_base_t *imu_h, uint32_t now_ms);
/**
 * @brief 处理 WHT101 接收完成并解析帧。
 */
static void imu_wht101_rx_complete_cb(uart_base_t *uart_h, void *context);
/**
 * @brief 处理 WHT101 UART 错误并触发重同步。
 */
static void imu_wht101_uart_err_cb(uart_base_t *uart_h, void *context);

/**
 * @brief 复制 WHT101 状态快照。
 */
static void imu_wht101_get_status(
  imu_base_t *imu_h,
  imu_status_t *status
) {
  assert(imu_h && status);
  imu_wht101_uart_t *imu_wht101_uart_h = (imu_wht101_uart_t *)imu_h;

  uint64_t error_count =
    (uint64_t)imu_wht101_uart_h->rx_error_count +
    (uint64_t)imu_wht101_uart_h->rx_overflow_count +
    (uint64_t)imu_wht101_uart_h->rx_checksum_error_count;

  if (error_count > UINT32_MAX) {
    error_count = UINT32_MAX;
  }

  *status = (imu_status_t){
    .is_started      = imu_wht101_uart_h->is_started,
    .is_busy         =
      imu_wht101_uart_h->config_state != IMU_WHT101_CONFIG_IDLE ||
      imu_wht101_uart_h->zero_pending ||
      imu_wht101_uart_h->rate_pending,
    .last_request_failed = imu_wht101_uart_h->config_failed,
    .error_count     = (uint32_t)error_count,
  };
}

/**
 * @brief 请求 WHT101 设置航向零点。
 */
static void imu_wht101_set_zero(imu_base_t *imu_h) {
  imu_wht101_uart_t *imu_wht101_uart_h = (imu_wht101_uart_t*)imu_h;
  Dev_IMU_WHT101_UART_SetZero(imu_wht101_uart_h);
}

static const imu_ops_t imu_ops_wht101 = {
  .start      = imu_wht101_start,
  .process    = imu_wht101_process,
  .get_status = imu_wht101_get_status,
  .set_zero   = imu_wht101_set_zero,
};

/******************************************************************************/

/**
 * @brief 写入 WHT101 配置寄存器。
 */
static dev_uart_status_t imu_wht101_write_reg(
  imu_wht101_uart_t *imu_wht101_uart_h,
  imu_wht101_reg_t reg,
  uint16_t val
) {
  uint8_t cmd[IMU_WHT101_WRITE_LEN] = {
    IMU_WHT101_WRITE_HDR1,
    IMU_WHT101_WRITE_HDR2,
    (uint8_t)reg,
    (uint8_t)(val & 0xFF),
    (uint8_t)((val >> 8) & 0xFF)
  };
  return Dev_UART_Base_Transmit(
    imu_wht101_uart_h->uart_h,
    cmd,
    IMU_WHT101_WRITE_LEN,
    10u
  );
}

/******************************************************************************/

/**
 * @brief 解析 WHT101 角度字段。
 */
static float imu_wht101_parse_angle(const imu_wht101_uart_t *imu_wht101_uart_h) {
  // 角度帧: 55 53 00 00 00 00 YawL YawH VL VH SUM
  int16_t yaw_raw = (int16_t)((imu_wht101_uart_h->frame[7] << 8) | imu_wht101_uart_h->frame[6]);
  return yaw_raw / IMU_WHT101_SCALE * IMU_WHT101_YAW_RANGE;
}

/**
 * @brief 解析 WHT101 角速度字段。
 */
static float imu_wht101_parse_gyro(const imu_wht101_uart_t *imu_wht101_uart_h) {
  // 角速度帧: 55 52 00 00 RWzL RWzH WzL WzH 00 00 SUM
  int16_t wz_raw = (int16_t)((imu_wht101_uart_h->frame[7] << 8) | imu_wht101_uart_h->frame[6]);
  return wz_raw / IMU_WHT101_SCALE * IMU_WHT101_GYRO_RANGE;
}

/**
 * @brief 把已解析通道写入样本并更新时间。
 */
static void imu_wht101_submit_channel(
  imu_wht101_uart_t *imu_wht101_uart_h,
  imu_channel_index_t channel,
  float value,
  uint32_t now_ms
) {
  filter_sample_t sample = {
    .valid_mask = (uint32_t)1u << channel,
    .time_ms = now_ms,
  };
  sample.value[channel] = value;
  (void)Dev_IMU_Base_SubmitSample(&imu_wht101_uart_h->imu_base, &sample);
}

/******************************************************************************/

/**
 * @brief 初始化 WHT101。
 * @param imu_h 对象。
 * @param name 名称。
 * @param uart_h 已初始化的 uart_base 对象，需支持中断接收。
 * @param filter_h 已初始化的静态滤波器根节点；WHT101 通常注入 passthrough。
 */
void Dev_IMU_WHT101_UART_Init(
  imu_wht101_uart_t *imu_h,
  const char *name,
  uart_base_t *uart_h,
  filter_base_t *filter_h
) {
  assert(imu_h && uart_h && filter_h);

  Dev_IMU_Base_Init(&imu_h->imu_base, name, &imu_ops_wht101, filter_h);
  imu_h->uart_h                   = uart_h;
  imu_h->state                   = IMU_WHT101_STATE_SYNCING_1;
  imu_h->frame_idx               = 0;
  imu_h->is_started              = false;
  imu_h->config_state            = IMU_WHT101_CONFIG_IDLE;
  imu_h->config_deadline_ms      = 0u;
  imu_h->zero_pending            = false;
  imu_h->rate_pending            = false;
  imu_h->config_failed           = false;
  imu_h->rate_pending_value      = 0u;
  imu_h->rx_byte                 = 0;
  imu_h->rx_write                = 0;
  imu_h->rx_read                 = 0;
  imu_h->rx_byte_count           = 0;
  imu_h->rx_overflow_count       = 0;
  imu_h->rx_error_count          = 0;
  imu_h->rx_checksum_error_count = 0;
  imu_h->rx_resync_requested     = false;
}

/**
 * @brief 重新挂接 WHT101 UART 接收。
 */
static void imu_wht101_rearm_receive(imu_wht101_uart_t *imu_wht101_uart_h) {
  assert(imu_wht101_uart_h);
  dev_uart_status_t status = Dev_UART_ReceiveIT(
    imu_wht101_uart_h->uart_h,
    &imu_wht101_uart_h->rx_byte,
    1u
  );
  assert(status == DEV_UART_OK);
  (void)status;
}

/**
 * @brief 处理 WHT101 接收完成并解析帧。
 */
static void imu_wht101_rx_complete_cb(uart_base_t *uart_h, void *context) {
  imu_wht101_uart_t *imu_wht101_uart_h = (imu_wht101_uart_t *)context;
  assert(imu_wht101_uart_h && uart_h == imu_wht101_uart_h->uart_h);
  (void)uart_h;

  imu_wht101_uart_h->rx_byte_count++;

  uint16_t next_write =
    (imu_wht101_uart_h->rx_write + 1u) & IMU_WHT101_RX_INDEX_MASK;

  if (next_write == imu_wht101_uart_h->rx_read) {
    imu_wht101_uart_h->rx_overflow_count++;
    imu_wht101_uart_h->rx_resync_requested = true;
  } else {
    imu_wht101_uart_h->rx_buffer[imu_wht101_uart_h->rx_write] = imu_wht101_uart_h->rx_byte;
    imu_wht101_uart_h->rx_write = next_write;
  }

  imu_wht101_rearm_receive(imu_wht101_uart_h);
}

/**
 * @brief 处理 WHT101 UART 错误并触发重同步。
 */
static void imu_wht101_uart_err_cb(uart_base_t *uart_h, void *context) {
  imu_wht101_uart_t *imu_wht101_uart_h = (imu_wht101_uart_t *)context;
  assert(imu_wht101_uart_h && uart_h == imu_wht101_uart_h->uart_h);
  (void)uart_h;

  imu_wht101_uart_h->rx_error_count++;
  imu_wht101_uart_h->rx_resync_requested = true;

  imu_wht101_rearm_receive(imu_wht101_uart_h);
}

/**
 * @brief 应用重同步结果并恢复接收。
 */
static void imu_wht101_apply_resync(imu_wht101_uart_t *imu_wht101_uart_h) {
  assert(imu_wht101_uart_h);

  if (!imu_wht101_uart_h->rx_resync_requested) {
    return;
  }

  imu_wht101_uart_h->state = IMU_WHT101_STATE_SYNCING_1;
  imu_wht101_uart_h->frame_idx = 0;
  imu_wht101_uart_h->rx_resync_requested = false;
}

/**
 * @brief 判断毫秒截止时间是否已到达。
 */
static bool imu_wht101_time_reached(uint32_t now_ms, uint32_t deadline_ms) {
  return (int32_t)(now_ms - deadline_ms) >= 0;
}

/**
 * @brief 写配置并记录请求状态。
 */
static bool imu_wht101_config_write(
  imu_wht101_uart_t *imu_wht101_uart_h,
  imu_wht101_reg_t reg,
  uint16_t value
) {
  dev_uart_status_t status = imu_wht101_write_reg(imu_wht101_uart_h, reg, value);
  if (status != DEV_UART_OK) {
    imu_wht101_uart_h->rx_error_count++;
    imu_wht101_uart_h->config_failed = true;
    return false;
  }
  return true;
}

/**
 * @brief 启动下一项速率配置请求。
 */
static void imu_wht101_config_start_next(
  imu_wht101_uart_t *imu_wht101_uart_h,
  uint32_t now_ms
) {
  assert(imu_wht101_uart_h);

  if (imu_wht101_uart_h->config_state != IMU_WHT101_CONFIG_IDLE) {
    return;
  }

  if (imu_wht101_uart_h->zero_pending) {
    imu_wht101_uart_h->zero_pending = false;

    if (!imu_wht101_config_write(
          imu_wht101_uart_h,
          IMU_WHT101_REG_KEY,
          IMU_WHT101_KEY_UNLOCK)) {
      return;
    }

    imu_wht101_uart_h->config_state = IMU_WHT101_CONFIG_ZERO_WAIT_CALIYAW;
    imu_wht101_uart_h->config_deadline_ms = now_ms + IMU_WHT101_DELAY_UNLOCK_MS;
    return;
  }

  if (imu_wht101_uart_h->rate_pending) {
    imu_wht101_uart_h->rate_pending = false;

    if (!imu_wht101_config_write(
          imu_wht101_uart_h,
          IMU_WHT101_REG_KEY,
          IMU_WHT101_KEY_UNLOCK)) {
      return;
    }

    imu_wht101_uart_h->config_state = IMU_WHT101_CONFIG_RATE_WAIT_WRITE;
    imu_wht101_uart_h->config_deadline_ms = now_ms + IMU_WHT101_DELAY_UNLOCK_MS;
  }
}

/**
 * @brief 推进 WHT101 配置状态机。
 */
static void imu_wht101_config_process(
  imu_wht101_uart_t *imu_wht101_uart_h,
  uint32_t now_ms
) {
  assert(imu_wht101_uart_h);

  if (
    imu_wht101_uart_h->config_state == IMU_WHT101_CONFIG_IDLE ||
    !imu_wht101_time_reached(now_ms, imu_wht101_uart_h->config_deadline_ms)
  ) {
    imu_wht101_config_start_next(imu_wht101_uart_h, now_ms);
    return;
  }

  switch (imu_wht101_uart_h->config_state) {
    case IMU_WHT101_CONFIG_ZERO_WAIT_CALIYAW:
      if (!imu_wht101_config_write(
            imu_wht101_uart_h,
            IMU_WHT101_REG_CALIYAW,
            IMU_WHT101_CALIYAW_ZERO)) {
        imu_wht101_uart_h->config_state = IMU_WHT101_CONFIG_IDLE;
        return;
      }
      imu_wht101_uart_h->config_state = IMU_WHT101_CONFIG_ZERO_WAIT_SAVE;
      imu_wht101_uart_h->config_deadline_ms = now_ms + IMU_WHT101_DELAY_SAVE_MS;
      break;

    case IMU_WHT101_CONFIG_ZERO_WAIT_SAVE:
      if (!imu_wht101_config_write(
            imu_wht101_uart_h,
            IMU_WHT101_REG_SAVE,
            IMU_WHT101_SAVE_VALUE)) {
        imu_wht101_uart_h->config_state = IMU_WHT101_CONFIG_IDLE;
        return;
      }
      imu_wht101_uart_h->config_state = IMU_WHT101_CONFIG_IDLE;
      imu_wht101_uart_h->config_failed = false;
      break;

    case IMU_WHT101_CONFIG_RATE_WAIT_WRITE:
      if (!imu_wht101_config_write(
            imu_wht101_uart_h,
            IMU_WHT101_REG_RRATE,
            imu_wht101_uart_h->rate_pending_value)) {
        imu_wht101_uart_h->config_state = IMU_WHT101_CONFIG_IDLE;
        return;
      }
      imu_wht101_uart_h->config_state = IMU_WHT101_CONFIG_RATE_WAIT_SAVE;
      imu_wht101_uart_h->config_deadline_ms = now_ms + IMU_WHT101_DELAY_WRITE_MS;
      break;

    case IMU_WHT101_CONFIG_RATE_WAIT_SAVE:
      if (!imu_wht101_config_write(
            imu_wht101_uart_h,
            IMU_WHT101_REG_SAVE,
            IMU_WHT101_SAVE_VALUE)) {
        imu_wht101_uart_h->config_state = IMU_WHT101_CONFIG_IDLE;
        return;
      }
      imu_wht101_uart_h->config_state = IMU_WHT101_CONFIG_IDLE;
      imu_wht101_uart_h->config_failed = false;
      break;

    case IMU_WHT101_CONFIG_IDLE:
    default:
      assert(0);
      break;
  }
}

/**
 * @brief 启动 WHT101 接收与配置。
 */
static void imu_wht101_start(imu_base_t *imu_h) {
  assert(imu_h);
  imu_wht101_uart_t *imu_wht101_uart_h = (imu_wht101_uart_t *)imu_h;

  assert(!imu_wht101_uart_h->is_started);

  dev_uart_callbacks_t callbacks = {
    .rx_complete_cb = imu_wht101_rx_complete_cb,
    .err_cb     = imu_wht101_uart_err_cb,
    .context    = imu_wht101_uart_h,
  };

  Dev_UART_RegisterCallback(imu_wht101_uart_h->uart_h, &callbacks);
  imu_wht101_rearm_receive(imu_wht101_uart_h);

  imu_wht101_uart_h->is_started = true;
}

/**
 * @brief 推进 WHT101 周期处理。
 */
static void imu_wht101_process(imu_base_t *imu_h, uint32_t now_ms) {
  assert(imu_h);
  imu_wht101_uart_t *imu_wht101_uart_h = (imu_wht101_uart_t *)imu_h;

  while (imu_wht101_uart_h->rx_read != imu_wht101_uart_h->rx_write) {
    imu_wht101_apply_resync(imu_wht101_uart_h);

    uint8_t byte = imu_wht101_uart_h->rx_buffer[imu_wht101_uart_h->rx_read];
    imu_wht101_uart_h->rx_read =
      (imu_wht101_uart_h->rx_read + 1u) & IMU_WHT101_RX_INDEX_MASK;
    Dev_IMU_WHT101_UART_FeedByte(imu_wht101_uart_h, byte, now_ms);
  }

  imu_wht101_apply_resync(imu_wht101_uart_h);
  imu_wht101_config_process(imu_wht101_uart_h, now_ms);
}

/**
 * @brief 解析一个字节。
 * @param imu_h 对象。
 * @param byte 输入字节。
 * @param now_ms 当前毫秒时间戳。
 * @note 该函数是解析器，不是 ISR 入队接口；应在任务上下文调用。
 */
void Dev_IMU_WHT101_UART_FeedByte(
  imu_wht101_uart_t *imu_h,
  uint8_t byte,
  uint32_t now_ms
) {
  assert(imu_h);

  switch (imu_h->state) {
    case IMU_WHT101_STATE_SYNCING_1:
      if (byte == IMU_WHT101_FRAME_HEADER) {
        imu_h->frame[0] = byte;
        imu_h->frame_idx = 1;
        imu_h->state = IMU_WHT101_STATE_SYNCING_2;
      }
      break;

    case IMU_WHT101_STATE_SYNCING_2:
      if (byte == IMU_WHT101_TYPE_GYRO || byte == IMU_WHT101_TYPE_ANGLE) {
        imu_h->frame[1] = byte;
        imu_h->frame_idx = 2;
        imu_h->state = IMU_WHT101_STATE_RECEIVING_DATA;
      } else if (byte == IMU_WHT101_FRAME_HEADER) {
        imu_h->frame[0] = byte;
        imu_h->frame_idx = 1;
      } else {
        imu_h->state = IMU_WHT101_STATE_SYNCING_1;
      }
      break;

    case IMU_WHT101_STATE_RECEIVING_DATA:
      imu_h->frame[imu_h->frame_idx++] = byte;
      if (imu_h->frame_idx == IMU_WHT101_FRAME_LEN - 1) {
        imu_h->state = IMU_WHT101_STATE_CALCULATING_SUM;
      }
      break;

    case IMU_WHT101_STATE_CALCULATING_SUM: {
      imu_h->frame[IMU_WHT101_FRAME_LEN - 1] = byte;
      uint8_t sum = 0;
      for (int i = 0; i < IMU_WHT101_FRAME_LEN - 1; i++) sum += imu_h->frame[i];
      if (sum == imu_h->frame[IMU_WHT101_FRAME_LEN - 1]) {
        if (imu_h->frame[1] == IMU_WHT101_TYPE_GYRO) {
          imu_wht101_submit_channel(
            imu_h,
            IMU_CHANNEL_INDEX_GYRO_Z,
            imu_wht101_parse_gyro(imu_h),
            now_ms
          );
        } else if (imu_h->frame[1] == IMU_WHT101_TYPE_ANGLE) {
          imu_wht101_submit_channel(
            imu_h,
            IMU_CHANNEL_INDEX_YAW,
            imu_wht101_parse_angle(imu_h),
            now_ms
          );
        }
      } else {
        imu_h->rx_checksum_error_count++;
      }
      imu_h->state = IMU_WHT101_STATE_SYNCING_1;
      break;
    }
  }
}

/**
 * @brief 请求设置 yaw 零点。
 * @note 非阻塞；实际写入由 process 状态机推进。
 */
void Dev_IMU_WHT101_UART_SetZero(imu_wht101_uart_t *imu_h) {
  assert(imu_h && imu_h->is_started);
  imu_h->config_failed = false;
  imu_h->zero_pending = true;
}

/**
 * @brief 请求设置输出速率。
 * @param imu_h 对象。
 * @param rate 速率配置值。
 * @note 非阻塞；实际写入由 process 状态机推进。
 */
void Dev_IMU_WHT101_UART_SetOutputRate(imu_wht101_uart_t *imu_h, uint8_t rate) {
  assert(imu_h && imu_h->is_started);
  imu_h->config_failed = false;
  imu_h->rate_pending_value = rate;
  imu_h->rate_pending = true;
}
