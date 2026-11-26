/**
 * @file imu_mpu6050.c
 * @brief MPU6050 I2C 6 轴 IMU 驱动实现。
 * @ingroup drivers
 */

#include "imu_mpu6050.h"

#if defined(HAL_I2C_MODULE_ENABLED) || defined(BSP_HOST_STUB)

#include "sys_critical.h"
#include "sys_time_window.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

#define MPU6050_REG_SMPLRT_DIV 0x19U
#define MPU6050_REG_CONFIG 0x1AU
#define MPU6050_REG_GYRO_CONFIG 0x1BU
#define MPU6050_REG_ACCEL_CONFIG 0x1CU
#define MPU6050_REG_INT_ENABLE 0x38U
#define MPU6050_REG_ACCEL_XOUT_H 0x3BU
#define MPU6050_REG_USER_CTRL 0x6AU
#define MPU6050_REG_PWR_MGMT_1 0x6BU
#define MPU6050_REG_PWR_MGMT_2 0x6CU
#define MPU6050_REG_WHO_AM_I 0x75U

#define MPU6050_PWR1_DEVICE_RESET 0x80U
#define MPU6050_PWR1_CLOCK_PLL_XGYRO 0x01U
#define MPU6050_SAMPLE_PERIOD_MIN_MS 1U
#define MPU6050_RESET_DELAY_MS 100U
#define MPU6050_BURST_SIZE 14U
#define MPU6050_GRAVITY_MPS2 9.80665f
#define MPU6050_CAL_MAX_STD_DPS 3.0f
#define MPU6050_CAL_MIN_ACCEL_MPS2 3.0f
#define MPU6050_CAL_MAX_ACCEL_MPS2 15.0f

static bsp_status_t mpu6050_write_reg(imu_mpu6050_t *imu, uint8_t reg, uint8_t value) {
  bsp_status_t st = BSP_I2C_MemWrite(&imu->cfg.i2c, reg, 1U, &value, 1U);
  if (st != BSP_OK) {
    imu->i2c_error_count++;
    imu->last_request_failed = true;
  }
  return st;
}

static bsp_status_t mpu6050_read_regs(imu_mpu6050_t *imu, uint8_t reg, uint8_t *data,
                                      uint16_t size) {
  bsp_status_t st = BSP_I2C_MemRead(&imu->cfg.i2c, reg, 1U, data, size);
  if (st != BSP_OK) {
    imu->i2c_error_count++;
    imu->last_request_failed = true;
  }
  return st;
}

static int16_t mpu6050_be_i16(const uint8_t *data) {
  int32_t raw = ((int32_t)data[0] << 8) | (int32_t)data[1];
  if ((raw & 0x8000) != 0) {
    raw -= 0x10000;
  }
  return (int16_t)raw;
}

static float mpu6050_accel_lsb(mpu6050_accel_range_t range) {
  switch (range) {
  case MPU6050_ACCEL_RANGE_2G:
    return 16384.0f;
  case MPU6050_ACCEL_RANGE_4G:
    return 8192.0f;
  case MPU6050_ACCEL_RANGE_8G:
    return 4096.0f;
  case MPU6050_ACCEL_RANGE_16G:
    return 2048.0f;
  default:
    return 0.0f;
  }
}

static float mpu6050_gyro_lsb(mpu6050_gyro_range_t range) {
  switch (range) {
  case MPU6050_GYRO_RANGE_250DPS:
    return 131.0f;
  case MPU6050_GYRO_RANGE_500DPS:
    return 65.5f;
  case MPU6050_GYRO_RANGE_1000DPS:
    return 32.8f;
  case MPU6050_GYRO_RANGE_2000DPS:
    return 16.4f;
  default:
    return 0.0f;
  }
}

static uint32_t mpu6050_gyro_rate_hz(mpu6050_dlpf_t dlpf) {
  return (dlpf == MPU6050_DLPF_256HZ) ? 8000U : 1000U;
}

static void mpu6050_apply_axis_map(const imu_mpu6050_cfg_t *cfg, const float input[3],
                                   float output[3]) {
  for (uint8_t axis = 0U; axis < 3U; ++axis) {
    uint8_t src = cfg->axis_source[axis];
    output[axis] = input[src] * (float)cfg->axis_sign[axis];
  }
}

static bsp_status_t mpu6050_acquire(imu_mpu6050_t *imu, uint32_t now_ms,
                                    mpu6050_raw_sample_t *sample) {
  uint8_t data[MPU6050_BURST_SIZE] = {0};
  bsp_status_t st = mpu6050_read_regs(imu, MPU6050_REG_ACCEL_XOUT_H, data, MPU6050_BURST_SIZE);
  if (st != BSP_OK) {
    return st;
  }

  float accel_g[3] = {
      (float)mpu6050_be_i16(&data[0]) / imu->accel_lsb_per_g,
      (float)mpu6050_be_i16(&data[2]) / imu->accel_lsb_per_g,
      (float)mpu6050_be_i16(&data[4]) / imu->accel_lsb_per_g,
  };
  float gyro_dps[3] = {
      (float)mpu6050_be_i16(&data[8]) / imu->gyro_lsb_per_dps,
      (float)mpu6050_be_i16(&data[10]) / imu->gyro_lsb_per_dps,
      (float)mpu6050_be_i16(&data[12]) / imu->gyro_lsb_per_dps,
  };

  float mapped_accel_g[3] = {0};
  float mapped_gyro_dps[3] = {0};
  mpu6050_apply_axis_map(&imu->cfg, accel_g, mapped_accel_g);
  mpu6050_apply_axis_map(&imu->cfg, gyro_dps, mapped_gyro_dps);

  for (uint8_t axis = 0U; axis < 3U; ++axis) {
    sample->accel_mps2[axis] = mapped_accel_g[axis] * MPU6050_GRAVITY_MPS2;
    sample->gyro_dps[axis] = mapped_gyro_dps[axis];
  }

  int16_t temp_raw = mpu6050_be_i16(&data[6]);
  sample->temperature_c = ((float)temp_raw / 340.0f) + 36.53f;
  sample->time_ms = now_ms;
  return BSP_OK;
}

static bool mpu6050_accel_is_stationary(const mpu6050_raw_sample_t *sample) {
  float norm_sq = 0.0f;
  for (uint8_t axis = 0U; axis < 3U; ++axis) {
    norm_sq += sample->accel_mps2[axis] * sample->accel_mps2[axis];
  }
  float norm = sqrtf(norm_sq);
  return norm >= MPU6050_CAL_MIN_ACCEL_MPS2 && norm <= MPU6050_CAL_MAX_ACCEL_MPS2;
}

static void mpu6050_cal_reset(imu_mpu6050_t *imu) {
  imu->calibration_count = 0U;
  for (uint8_t axis = 0U; axis < 3U; ++axis) {
    imu->calibration_gyro_sum[axis] = 0.0f;
    imu->calibration_gyro_sum_sq[axis] = 0.0f;
  }
}

static bool mpu6050_cal_finish(imu_mpu6050_t *imu) {
  float count = (float)imu->calibration_count;
  for (uint8_t axis = 0U; axis < 3U; ++axis) {
    float mean = imu->calibration_gyro_sum[axis] / count;
    float variance = (imu->calibration_gyro_sum_sq[axis] / count) - (mean * mean);
    if (variance < 0.0f) {
      variance = 0.0f;
    }
    if (sqrtf(variance) > MPU6050_CAL_MAX_STD_DPS) {
      return false;
    }
    imu->gyro_bias_dps[axis] = mean;
  }
  return true;
}

static bsp_status_t mpu6050_configure(imu_mpu6050_t *imu) {
  const uint8_t gyro_cfg = (uint8_t)((uint8_t)imu->cfg.gyro_range << 3U);
  const uint8_t accel_cfg = (uint8_t)((uint8_t)imu->cfg.accel_range << 3U);

  bsp_status_t st = mpu6050_write_reg(imu, MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_CLOCK_PLL_XGYRO);
  if (st != BSP_OK) {
    return st;
  }
  st = mpu6050_write_reg(imu, MPU6050_REG_SMPLRT_DIV, imu->sample_rate_divider);
  if (st != BSP_OK) {
    return st;
  }
  st = mpu6050_write_reg(imu, MPU6050_REG_CONFIG, (uint8_t)imu->cfg.dlpf);
  if (st != BSP_OK) {
    return st;
  }
  st = mpu6050_write_reg(imu, MPU6050_REG_GYRO_CONFIG, gyro_cfg);
  if (st != BSP_OK) {
    return st;
  }
  st = mpu6050_write_reg(imu, MPU6050_REG_ACCEL_CONFIG, accel_cfg);
  if (st != BSP_OK) {
    return st;
  }
  st = mpu6050_write_reg(imu, MPU6050_REG_INT_ENABLE, 0U);
  if (st != BSP_OK) {
    return st;
  }
  st = mpu6050_write_reg(imu, MPU6050_REG_USER_CTRL, 0U);
  if (st != BSP_OK) {
    return st;
  }
  st = mpu6050_write_reg(imu, MPU6050_REG_PWR_MGMT_2, 0U);
  if (st != BSP_OK) {
    return st;
  }

  uint8_t rb = 0U;
  st = mpu6050_read_regs(imu, MPU6050_REG_SMPLRT_DIV, &rb, 1U);
  if (st != BSP_OK || rb != imu->sample_rate_divider) {
    return (st != BSP_OK) ? st : BSP_ERR_HAL;
  }
  st = mpu6050_read_regs(imu, MPU6050_REG_CONFIG, &rb, 1U);
  if (st != BSP_OK || (rb & 0x07U) != (uint8_t)imu->cfg.dlpf) {
    return (st != BSP_OK) ? st : BSP_ERR_HAL;
  }
  st = mpu6050_read_regs(imu, MPU6050_REG_GYRO_CONFIG, &rb, 1U);
  if (st != BSP_OK || (rb & 0x18U) != gyro_cfg) {
    return (st != BSP_OK) ? st : BSP_ERR_HAL;
  }
  st = mpu6050_read_regs(imu, MPU6050_REG_ACCEL_CONFIG, &rb, 1U);
  if (st != BSP_OK || (rb & 0x18U) != accel_cfg) {
    return (st != BSP_OK) ? st : BSP_ERR_HAL;
  }
  st = mpu6050_read_regs(imu, MPU6050_REG_PWR_MGMT_1, &rb, 1U);
  if (st != BSP_OK || (rb & 0x7FU) != MPU6050_PWR1_CLOCK_PLL_XGYRO) {
    return (st != BSP_OK) ? st : BSP_ERR_HAL;
  }

  return BSP_OK;
}

static void mpu6050_enter_error(imu_mpu6050_t *imu) {
  imu->state = MPU6050_STATE_ERROR;
  imu->last_request_failed = true;
}

static void mpu6050_process_calibration(imu_mpu6050_t *imu, uint32_t now_ms) {
  if (!Sys_Time_HasReachedMs(now_ms, imu->next_sample_ms)) {
    return;
  }
  imu->next_sample_ms = now_ms + imu->sample_period_ms;

  mpu6050_raw_sample_t sample = {0};
  if (mpu6050_acquire(imu, now_ms, &sample) != BSP_OK || !mpu6050_accel_is_stationary(&sample)) {
    imu->calibration_error_count++;
    mpu6050_enter_error(imu);
    return;
  }

  for (uint8_t axis = 0U; axis < 3U; ++axis) {
    imu->calibration_gyro_sum[axis] += sample.gyro_dps[axis];
    imu->calibration_gyro_sum_sq[axis] += sample.gyro_dps[axis] * sample.gyro_dps[axis];
  }
  imu->calibration_count++;

  if (imu->calibration_count < imu->cfg.calibration_samples) {
    return;
  }
  if (!mpu6050_cal_finish(imu)) {
    imu->calibration_error_count++;
    mpu6050_enter_error(imu);
    return;
  }

  imu->state = MPU6050_STATE_READY;
  imu->next_sample_ms = now_ms;
  imu->last_request_failed = false;
}

static void mpu6050_process_ready(imu_mpu6050_t *imu, uint32_t now_ms) {
  if (!Sys_Time_HasReachedMs(now_ms, imu->next_sample_ms)) {
    return;
  }
  imu->next_sample_ms = now_ms + imu->sample_period_ms;

  mpu6050_raw_sample_t raw = {0};
  if (mpu6050_acquire(imu, now_ms, &raw) != BSP_OK) {
    imu->last_request_failed = true;
    return;
  }

  for (uint8_t axis = 0U; axis < 3U; ++axis) {
    raw.gyro_dps[axis] -= imu->gyro_bias_dps[axis];
  }

  Sys_Critical_Enter();
  imu->raw_sample = raw;
  imu->has_raw_sample = true;
  Sys_Critical_Exit();

  filter_sample_t input = {
      .valid_mask = IMU_CHANNEL_ACCEL_X | IMU_CHANNEL_ACCEL_Y | IMU_CHANNEL_ACCEL_Z |
                    IMU_CHANNEL_GYRO_X | IMU_CHANNEL_GYRO_Y | IMU_CHANNEL_GYRO_Z,
      .time_ms = now_ms,
  };
  input.value[IMU_CHANNEL_INDEX_ACCEL_X] = raw.accel_mps2[0];
  input.value[IMU_CHANNEL_INDEX_ACCEL_Y] = raw.accel_mps2[1];
  input.value[IMU_CHANNEL_INDEX_ACCEL_Z] = raw.accel_mps2[2];
  input.value[IMU_CHANNEL_INDEX_GYRO_X] = raw.gyro_dps[0];
  input.value[IMU_CHANNEL_INDEX_GYRO_Y] = raw.gyro_dps[1];
  input.value[IMU_CHANNEL_INDEX_GYRO_Z] = raw.gyro_dps[2];

  if (Dev_IMU_Base_SubmitSample(&imu->base, &input)) {
    imu->last_request_failed = false;
  } else {
    imu->filter_reject_count++;
    imu->last_request_failed = true;
  }
}

static void mpu6050_start_op(imu_base_t *imu_h) {
  imu_mpu6050_t *imu = (imu_mpu6050_t *)imu_h;
  if (imu == NULL || imu->is_started) {
    return;
  }
  imu->is_started = true;
  imu->state = MPU6050_STATE_RESET_PENDING;
  imu->last_request_failed = false;
}

static void mpu6050_process_op(imu_base_t *imu_h, uint32_t now_ms) {
  imu_mpu6050_t *imu = (imu_mpu6050_t *)imu_h;
  if (imu == NULL || !imu->is_started) {
    return;
  }

  switch (imu->state) {
  case MPU6050_STATE_RESET_PENDING:
    if (mpu6050_write_reg(imu, MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_DEVICE_RESET) != BSP_OK) {
      mpu6050_enter_error(imu);
      return;
    }
    imu->state = MPU6050_STATE_RESET_WAIT;
    imu->state_deadline_ms = now_ms + MPU6050_RESET_DELAY_MS;
    break;

  case MPU6050_STATE_RESET_WAIT:
    if (!Sys_Time_HasReachedMs(now_ms, imu->state_deadline_ms)) {
      return;
    }
    if (mpu6050_configure(imu) != BSP_OK) {
      imu->configuration_error_count++;
      mpu6050_enter_error(imu);
      return;
    }
    imu->state = MPU6050_STATE_CALIBRATE_PENDING;
    break;

  case MPU6050_STATE_CALIBRATE_PENDING:
    mpu6050_cal_reset(imu);
    imu->next_sample_ms = now_ms;
    if (imu->cfg.calibration_samples == 0U) {
      for (uint8_t axis = 0U; axis < 3U; ++axis) {
        imu->gyro_bias_dps[axis] = 0.0f;
      }
      imu->state = MPU6050_STATE_READY;
    } else {
      imu->state = MPU6050_STATE_CALIBRATING;
    }
    break;

  case MPU6050_STATE_CALIBRATING:
    mpu6050_process_calibration(imu, now_ms);
    break;

  case MPU6050_STATE_READY:
    mpu6050_process_ready(imu, now_ms);
    break;

  case MPU6050_STATE_UNINITIALIZED:
  case MPU6050_STATE_ERROR:
  default:
    break;
  }
}

static void mpu6050_get_status_op(imu_base_t *imu_h, imu_status_t *status) {
  imu_mpu6050_t *imu = (imu_mpu6050_t *)imu_h;
  if (imu == NULL || status == NULL) {
    return;
  }

  uint64_t err_sum = (uint64_t)imu->i2c_error_count + (uint64_t)imu->configuration_error_count +
                     (uint64_t)imu->calibration_error_count + (uint64_t)imu->filter_reject_count;
  if (err_sum > UINT32_MAX) {
    err_sum = UINT32_MAX;
  }

  *status = (imu_status_t){
      .is_started = imu->is_started,
      .is_busy = (imu->state == MPU6050_STATE_RESET_PENDING) ||
                 (imu->state == MPU6050_STATE_RESET_WAIT) ||
                 (imu->state == MPU6050_STATE_CALIBRATE_PENDING) ||
                 (imu->state == MPU6050_STATE_CALIBRATING),
      .last_request_failed = imu->last_request_failed,
      .error_count = (uint32_t)err_sum,
  };
}

static void mpu6050_set_zero_op(imu_base_t *imu_h) { Dev_IMU_Base_ZeroYaw(imu_h); }

static const imu_ops_t s_mpu6050_ops = {
    .start = mpu6050_start_op,
    .process = mpu6050_process_op,
    .get_status = mpu6050_get_status_op,
    .set_zero = mpu6050_set_zero_op,
};

static bool mpu6050_cfg_isvalid(const imu_mpu6050_cfg_t *cfg) {
  if (cfg == NULL || cfg->name == NULL || cfg->filter == NULL || cfg->i2c.hi2c == NULL ||
      cfg->i2c.timeout_ms == 0U ||
      (cfg->i2c.addr_7bit != MPU6050_ADDRESS_AD0_LOW &&
       cfg->i2c.addr_7bit != MPU6050_ADDRESS_AD0_HIGH) ||
      cfg->dlpf > MPU6050_DLPF_5HZ || cfg->gyro_range > MPU6050_GYRO_RANGE_2000DPS ||
      cfg->accel_range > MPU6050_ACCEL_RANGE_16G || cfg->sample_rate_hz < 4U ||
      cfg->sample_rate_hz > 1000U || cfg->calibration_samples > 1000U) {
    return false;
  }

  uint32_t rate_hz = mpu6050_gyro_rate_hz(cfg->dlpf);
  uint32_t div = (rate_hz + (cfg->sample_rate_hz / 2U)) / cfg->sample_rate_hz;
  div = (div == 0U) ? 0U : (div - 1U);
  if (div > UINT8_MAX) {
    return false;
  }

  bool seen[3] = {false, false, false};
  for (uint8_t axis = 0U; axis < 3U; ++axis) {
    uint8_t src = cfg->axis_source[axis];
    int8_t sgn = cfg->axis_sign[axis];
    if (src >= 3U || seen[src] || (sgn != -1 && sgn != 1)) {
      return false;
    }
    seen[src] = true;
  }
  return true;
}

bsp_status_t Dev_IMU_MPU6050_Init(imu_mpu6050_t *imu, const imu_mpu6050_cfg_t *cfg) {
  if (imu == NULL || !mpu6050_cfg_isvalid(cfg)) {
    return BSP_ERR_INVALID_ARG;
  }

  (void)memset(imu, 0, sizeof(*imu));
  imu->cfg = *cfg;
  imu->state = MPU6050_STATE_UNINITIALIZED;
  imu->accel_lsb_per_g = mpu6050_accel_lsb(cfg->accel_range);
  imu->gyro_lsb_per_dps = mpu6050_gyro_lsb(cfg->gyro_range);

  uint32_t rate_hz = mpu6050_gyro_rate_hz(cfg->dlpf);
  uint32_t div = (rate_hz + (cfg->sample_rate_hz / 2U)) / cfg->sample_rate_hz;
  div = (div == 0U) ? 0U : (div - 1U);
  imu->sample_rate_divider = (uint8_t)div;

  uint32_t period_ms = (1000U + (cfg->sample_rate_hz / 2U)) / cfg->sample_rate_hz;
  imu->sample_period_ms =
      (period_ms < MPU6050_SAMPLE_PERIOD_MIN_MS) ? MPU6050_SAMPLE_PERIOD_MIN_MS : period_ms;

  uint8_t who_am_i = 0U;
  bsp_status_t st = mpu6050_read_regs(imu, MPU6050_REG_WHO_AM_I, &who_am_i, 1U);
  if (st != BSP_OK) {
    imu->last_request_failed = true;
    return st;
  }
  if (who_am_i != MPU6050_WHO_AM_I_VALUE && who_am_i != MPU6050_WHO_AM_I_ALT) {
    imu->last_request_failed = true;
    return BSP_ERR_HAL;
  }

  Dev_IMU_Base_Init(&imu->base, cfg->name, &s_mpu6050_ops, cfg->filter);
  imu->last_request_failed = false;
  return BSP_OK;
}

bool Dev_IMU_MPU6050_GetRawSample(const imu_mpu6050_t *imu, mpu6050_raw_sample_t *out_sample) {
  if (imu == NULL || out_sample == NULL) {
    return false;
  }
  Sys_Critical_Enter();
  *out_sample = imu->raw_sample;
  bool available = imu->has_raw_sample;
  Sys_Critical_Exit();
  return available;
}

bsp_status_t Dev_IMU_MPU6050_RequestGyroCalibration(imu_mpu6050_t *imu) {
  if (imu == NULL || !imu->is_started || imu->cfg.calibration_samples == 0U ||
      imu->state != MPU6050_STATE_READY) {
    return BSP_ERR_INVALID_ARG;
  }
  imu->last_request_failed = false;
  imu->state = MPU6050_STATE_CALIBRATE_PENDING;
  return BSP_OK;
}

#else

typedef int imu_mpu6050_empty_tu_t;

#endif /* HAL_I2C_MODULE_ENABLED || BSP_HOST_STUB */
