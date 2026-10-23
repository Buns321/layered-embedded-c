//
// Created by Buns_ on 2026/9/1.
//

#include "imu_wht101_usart.h"
#include <assert.h>
#include <stdint.h>

static void imu_wht101_start(imu_base_t *this);
static void imu_wht101_process(imu_base_t *this, uint32_t now_ms);
static void imu_wht101_rx_cplt_cb(usart_base_t *usart, void *context);
static void imu_wht101_uart_err_cb(usart_base_t *usart, void *context);

static void imu_wht101_get_status(
  imu_base_t *this,
  imu_status_t *status
) {
  assert(this && status);
  imu_wht101_usart_t *self = (imu_wht101_usart_t *)this;

  uint64_t error_count =
    (uint64_t)self->rx_error_count +
    (uint64_t)self->rx_overflow_count +
    (uint64_t)self->rx_checksum_error_count;

  if (error_count > UINT32_MAX) {
    error_count = UINT32_MAX;
  }

  *status = (imu_status_t){
    .is_started      = self->is_started,
    .is_busy         =
      self->config_state != IMU_WHT101_CONFIG_IDLE ||
      self->zero_pending ||
      self->rate_pending,
    .last_request_failed = self->config_failed,
    .error_count     = (uint32_t)error_count,
  };
}

static void imu_wht101_set_zero(imu_base_t *this) {
  imu_wht101_usart_t *self = (imu_wht101_usart_t*)this;
  Dev_IMU_WHT101_USART_SetZero(self);
}

static const imu_ops_t imu_ops_wht101 = {
  .start      = imu_wht101_start,
  .process    = imu_wht101_process,
  .get_status = imu_wht101_get_status,
  .set_zero   = imu_wht101_set_zero,
};

/******************************************************************************/

static Dev_USART_Status imu_wht101_write_reg(
  imu_wht101_usart_t *self,
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
  return Dev_USART_Base_Transmit(
    self->usart,
    cmd,
    IMU_WHT101_WRITE_LEN,
    10u
  );
}

/******************************************************************************/

static float imu_wht101_parse_angle(const imu_wht101_usart_t *self) {
  // 角度帧: 55 53 00 00 00 00 YawL YawH VL VH SUM
  int16_t yaw_raw = (int16_t)((self->frame[7] << 8) | self->frame[6]);
  return yaw_raw / IMU_WHT101_SCALE * IMU_WHT101_YAW_RANGE;
}

static float imu_wht101_parse_gyro(const imu_wht101_usart_t *self) {
  // 角速度帧: 55 52 00 00 RWzL RWzH WzL WzH 00 00 SUM
  int16_t wz_raw = (int16_t)((self->frame[7] << 8) | self->frame[6]);
  return wz_raw / IMU_WHT101_SCALE * IMU_WHT101_GYRO_RANGE;
}

static void imu_wht101_submit_channel(
  imu_wht101_usart_t *self,
  imu_channel_index_t channel,
  float value,
  uint32_t now_ms
) {
  filter_sample_t sample = {
    .valid_mask = (uint32_t)1u << channel,
    .time_ms = now_ms,
  };
  sample.value[channel] = value;
  (void)Dev_IMU_Base_SubmitSample(&self->imu_base, &sample);
}

/******************************************************************************/

void Dev_IMU_WHT101_USART_Init(
  imu_wht101_usart_t *this,
  const char *name,
  usart_base_t *usart,
  filter_base_t *filter
) {
  assert(this && usart && filter);

  Dev_IMU_Base_Init(&this->imu_base, name, &imu_ops_wht101, filter);
  this->usart                   = usart;
  this->state                   = IMU_WHT101_STATE_SYNCING_1;
  this->frame_idx               = 0;
  this->is_started              = false;
  this->config_state            = IMU_WHT101_CONFIG_IDLE;
  this->config_deadline_ms      = 0u;
  this->zero_pending            = false;
  this->rate_pending            = false;
  this->config_failed           = false;
  this->rate_pending_value      = 0u;
  this->rx_byte                 = 0;
  this->rx_write                = 0;
  this->rx_read                 = 0;
  this->rx_byte_count           = 0;
  this->rx_overflow_count       = 0;
  this->rx_error_count          = 0;
  this->rx_checksum_error_count = 0;
  this->rx_resync_requested     = false;
}

static void imu_wht101_rearm_receive(imu_wht101_usart_t *self) {
  assert(self);
  Dev_USART_Status status = Dev_USART_Receive_IT(
    self->usart,
    &self->rx_byte,
    1u
  );
  assert(status == DEV_USART_OK);
  (void)status;
}

static void imu_wht101_rx_cplt_cb(usart_base_t *usart, void *context) {
  imu_wht101_usart_t *self = (imu_wht101_usart_t *)context;
  assert(self && usart == self->usart);

  self->rx_byte_count++;

  uint16_t next_write =
    (self->rx_write + 1u) & IMU_WHT101_RX_INDEX_MASK;

  if (next_write == self->rx_read) {
    self->rx_overflow_count++;
    self->rx_resync_requested = true;
  } else {
    self->rx_buffer[self->rx_write] = self->rx_byte;
    self->rx_write = next_write;
  }

  imu_wht101_rearm_receive(self);
}

static void imu_wht101_uart_err_cb(usart_base_t *usart, void *context) {
  imu_wht101_usart_t *self = (imu_wht101_usart_t *)context;
  assert(self && usart == self->usart);

  self->rx_error_count++;
  self->rx_resync_requested = true;

  imu_wht101_rearm_receive(self);
}

static void imu_wht101_apply_resync(imu_wht101_usart_t *self) {
  assert(self);

  if (!self->rx_resync_requested) {
    return;
  }

  self->state = IMU_WHT101_STATE_SYNCING_1;
  self->frame_idx = 0;
  self->rx_resync_requested = false;
}

static bool imu_wht101_time_reached(uint32_t now_ms, uint32_t deadline_ms) {
  return (int32_t)(now_ms - deadline_ms) >= 0;
}

static bool imu_wht101_config_write(
  imu_wht101_usart_t *self,
  imu_wht101_reg_t reg,
  uint16_t value
) {
  Dev_USART_Status status = imu_wht101_write_reg(self, reg, value);
  if (status != DEV_USART_OK) {
    self->rx_error_count++;
    self->config_failed = true;
    return false;
  }
  return true;
}

static void imu_wht101_config_start_next(
  imu_wht101_usart_t *self,
  uint32_t now_ms
) {
  assert(self);

  if (self->config_state != IMU_WHT101_CONFIG_IDLE) {
    return;
  }

  if (self->zero_pending) {
    self->zero_pending = false;

    if (!imu_wht101_config_write(
          self,
          IMU_WHT101_REG_KEY,
          IMU_WHT101_KEY_UNLOCK)) {
      return;
    }

    self->config_state = IMU_WHT101_CONFIG_ZERO_WAIT_CALIYAW;
    self->config_deadline_ms = now_ms + IMU_WHT101_DELAY_UNLOCK_MS;
    return;
  }

  if (self->rate_pending) {
    self->rate_pending = false;

    if (!imu_wht101_config_write(
          self,
          IMU_WHT101_REG_KEY,
          IMU_WHT101_KEY_UNLOCK)) {
      return;
    }

    self->config_state = IMU_WHT101_CONFIG_RATE_WAIT_WRITE;
    self->config_deadline_ms = now_ms + IMU_WHT101_DELAY_UNLOCK_MS;
  }
}

static void imu_wht101_config_process(
  imu_wht101_usart_t *self,
  uint32_t now_ms
) {
  assert(self);

  if (
    self->config_state == IMU_WHT101_CONFIG_IDLE ||
    !imu_wht101_time_reached(now_ms, self->config_deadline_ms)
  ) {
    imu_wht101_config_start_next(self, now_ms);
    return;
  }

  switch (self->config_state) {
    case IMU_WHT101_CONFIG_ZERO_WAIT_CALIYAW:
      if (!imu_wht101_config_write(
            self,
            IMU_WHT101_REG_CALIYAW,
            IMU_WHT101_CALIYAW_ZERO)) {
        self->config_state = IMU_WHT101_CONFIG_IDLE;
        return;
      }
      self->config_state = IMU_WHT101_CONFIG_ZERO_WAIT_SAVE;
      self->config_deadline_ms = now_ms + IMU_WHT101_DELAY_SAVE_MS;
      break;

    case IMU_WHT101_CONFIG_ZERO_WAIT_SAVE:
      if (!imu_wht101_config_write(
            self,
            IMU_WHT101_REG_SAVE,
            IMU_WHT101_SAVE_VALUE)) {
        self->config_state = IMU_WHT101_CONFIG_IDLE;
        return;
      }
      self->config_state = IMU_WHT101_CONFIG_IDLE;
      self->config_failed = false;
      break;

    case IMU_WHT101_CONFIG_RATE_WAIT_WRITE:
      if (!imu_wht101_config_write(
            self,
            IMU_WHT101_REG_RRATE,
            self->rate_pending_value)) {
        self->config_state = IMU_WHT101_CONFIG_IDLE;
        return;
      }
      self->config_state = IMU_WHT101_CONFIG_RATE_WAIT_SAVE;
      self->config_deadline_ms = now_ms + IMU_WHT101_DELAY_WRITE_MS;
      break;

    case IMU_WHT101_CONFIG_RATE_WAIT_SAVE:
      if (!imu_wht101_config_write(
            self,
            IMU_WHT101_REG_SAVE,
            IMU_WHT101_SAVE_VALUE)) {
        self->config_state = IMU_WHT101_CONFIG_IDLE;
        return;
      }
      self->config_state = IMU_WHT101_CONFIG_IDLE;
      self->config_failed = false;
      break;

    case IMU_WHT101_CONFIG_IDLE:
    default:
      assert(0);
      break;
  }
}

static void imu_wht101_start(imu_base_t *this) {
  assert(this);
  imu_wht101_usart_t *self = (imu_wht101_usart_t *)this;

  assert(!self->is_started);

  dev_usart_callbacks_t callbacks = {
    .rx_cplt_cb = imu_wht101_rx_cplt_cb,
    .err_cb     = imu_wht101_uart_err_cb,
    .context    = self,
  };

  Dev_USART_RegisterCallbacks(self->usart, &callbacks);
  imu_wht101_rearm_receive(self);

  self->is_started = true;
}

static void imu_wht101_process(imu_base_t *this, uint32_t now_ms) {
  assert(this);
  imu_wht101_usart_t *self = (imu_wht101_usart_t *)this;

  while (self->rx_read != self->rx_write) {
    imu_wht101_apply_resync(self);

    uint8_t byte = self->rx_buffer[self->rx_read];
    self->rx_read =
      (self->rx_read + 1u) & IMU_WHT101_RX_INDEX_MASK;
    Dev_IMU_WHT101_USART_FeedByte(self, byte, now_ms);
  }

  imu_wht101_apply_resync(self);
  imu_wht101_config_process(self, now_ms);
}

void Dev_IMU_WHT101_USART_FeedByte(
  imu_wht101_usart_t *this,
  uint8_t byte,
  uint32_t now_ms
) {
  assert(this);

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
        if (this->frame[1] == IMU_WHT101_TYPE_GYRO) {
          imu_wht101_submit_channel(
            this,
            IMU_CHANNEL_INDEX_GYRO_Z,
            imu_wht101_parse_gyro(this),
            now_ms
          );
        } else if (this->frame[1] == IMU_WHT101_TYPE_ANGLE) {
          imu_wht101_submit_channel(
            this,
            IMU_CHANNEL_INDEX_YAW,
            imu_wht101_parse_angle(this),
            now_ms
          );
        }
      } else {
        this->rx_checksum_error_count++;
      }
      this->state = IMU_WHT101_STATE_SYNCING_1;
      break;
    }
  }
}

void Dev_IMU_WHT101_USART_SetZero(imu_wht101_usart_t *this) {
  assert(this && this->is_started);
  this->config_failed = false;
  this->zero_pending = true;
}

void Dev_IMU_WHT101_USART_SetOutputRate(imu_wht101_usart_t *this, uint8_t rate) {
  assert(this && this->is_started);
  this->config_failed = false;
  this->rate_pending_value = rate;
  this->rate_pending = true;
}
