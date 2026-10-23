//
// 由 Buns_ 创建于 2026/10/23。
//

#include "mpu6050_i2c.h"
#include "system_critical.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define MPU6050_REG_SMPLRT_DIV       0x19u
#define MPU6050_REG_CONFIG           0x1Au
#define MPU6050_REG_GYRO_CONFIG      0x1Bu
#define MPU6050_REG_ACCEL_CONFIG     0x1Cu
#define MPU6050_REG_INT_ENABLE       0x38u
#define MPU6050_REG_ACCEL_XOUT_H     0x3Bu
#define MPU6050_REG_PWR_MGMT_1       0x6Bu
#define MPU6050_REG_PWR_MGMT_2       0x6Cu
#define MPU6050_REG_USER_CTRL        0x6Au
#define MPU6050_REG_WHO_AM_I         0x75u

#define MPU6050_PWR1_DEVICE_RESET    0x80u
#define MPU6050_PWR1_CLOCK_PLL_XGYRO 0x01u
#define MPU6050_SAMPLE_PERIOD_MIN_MS 1u
#define MPU6050_RESET_DELAY_MS       100u
#define MPU6050_BURST_SIZE           14u
#define MPU6050_GRAVITY_MPS2         9.80665f
#define MPU6050_CALIBRATION_MAX_STD_DPS 3.0f
#define MPU6050_CALIBRATION_MIN_ACCEL_MPS2 3.0f
#define MPU6050_CALIBRATION_MAX_ACCEL_MPS2 15.0f

static void mpu6050_process(imu_base_t *imu_h, uint32_t now_ms);

static platform_status_t mpu6050_write_reg(
  mpu6050_i2c_t *mpu6050_i2c_h,
  uint8_t reg,
  uint8_t value
) {
  assert(mpu6050_i2c_h);

  platform_status_t status = Platform_I2C_MemWrite(
    mpu6050_i2c_h->i2c,
    mpu6050_i2c_h->address,
    reg,
    1u,
    &value,
    1u,
    mpu6050_i2c_h->config.i2c_timeout_ms
  );

  if (status != PLATFORM_STATUS_OK) {
    mpu6050_i2c_h->i2c_error_count++;
    mpu6050_i2c_h->last_request_failed = true;
  }

  return status;
}

static platform_status_t mpu6050_read_regs(
  mpu6050_i2c_t *mpu6050_i2c_h,
  uint8_t reg,
  uint8_t *data,
  uint16_t size
) {
  assert(mpu6050_i2c_h && data && size != 0u);

  platform_status_t status = Platform_I2C_MemRead(
    mpu6050_i2c_h->i2c,
    mpu6050_i2c_h->address,
    reg,
    1u,
    data,
    size,
    mpu6050_i2c_h->config.i2c_timeout_ms
  );

  if (status != PLATFORM_STATUS_OK) {
    mpu6050_i2c_h->i2c_error_count++;
    mpu6050_i2c_h->last_request_failed = true;
  }

  return status;
}

static platform_status_t mpu6050_read_reg(
  mpu6050_i2c_t *mpu6050_i2c_h,
  uint8_t reg,
  uint8_t *value
) {
  return mpu6050_read_regs(mpu6050_i2c_h, reg, value, 1u);
}

static bool mpu6050_time_reached(uint32_t now_ms, uint32_t deadline_ms) {
  return (int32_t)(now_ms - deadline_ms) >= 0;
}

static int16_t mpu6050_be_i16(const uint8_t *data) {
  assert(data);

  int32_t raw = ((int32_t)data[0] << 8) | data[1];
  if ((raw & 0x8000) != 0) {
    raw -= 0x10000;
  }

  return (int16_t)raw;
}

static float mpu6050_accel_lsb_per_g(mpu6050_accel_range_t range) {
  switch (range) {
    case MPU6050_ACCEL_RANGE_2G:  return 16384.0f;
    case MPU6050_ACCEL_RANGE_4G:  return 8192.0f;
    case MPU6050_ACCEL_RANGE_8G:  return 4096.0f;
    case MPU6050_ACCEL_RANGE_16G: return 2048.0f;
    default:                      assert(0); return 0.0f;
  }
}

static float mpu6050_gyro_lsb_per_dps(mpu6050_gyro_range_t range) {
  switch (range) {
    case MPU6050_GYRO_RANGE_250DPS:  return 131.0f;
    case MPU6050_GYRO_RANGE_500DPS:  return 65.5f;
    case MPU6050_GYRO_RANGE_1000DPS: return 32.8f;
    case MPU6050_GYRO_RANGE_2000DPS: return 16.4f;
    default:                         assert(0); return 0.0f;
  }
}

static uint32_t mpu6050_gyro_output_rate(mpu6050_dlpf_t dlpf) {
  return dlpf == MPU6050_DLPF_256HZ ? 8000u : 1000u;
}

static void mpu6050_apply_axis_map(
  const mpu6050_config_t *config,
  const float input[3],
  float output[3]
) {
  assert(config && input && output);

  for (uint8_t axis = 0u; axis < 3u; axis++) {
    uint8_t source = config->axis_source[axis];
    assert(source < 3u);
    assert(config->axis_sign[axis] == -1 || config->axis_sign[axis] == 1);
    output[axis] = input[source] * (float)config->axis_sign[axis];
  }
}

static void mpu6050_store_raw(
  mpu6050_i2c_t *mpu6050_i2c_h,
  const mpu6050_raw_sample_t *sample
) {
  assert(mpu6050_i2c_h && sample);

  Sys_Critical_Enter();
  mpu6050_i2c_h->raw_sample = *sample;
  mpu6050_i2c_h->has_raw_sample = true;
  Sys_Critical_Exit();
}

static void mpu6050_enter_error(mpu6050_i2c_t *mpu6050_i2c_h) {
  assert(mpu6050_i2c_h);

  mpu6050_i2c_h->state = MPU6050_STATE_ERROR;
  mpu6050_i2c_h->last_request_failed = true;
}

static platform_status_t mpu6050_acquire(
  mpu6050_i2c_t *mpu6050_i2c_h,
  uint32_t now_ms,
  mpu6050_raw_sample_t *sample
) {
  assert(mpu6050_i2c_h && sample);

  uint8_t data[MPU6050_BURST_SIZE] = {0};
  platform_status_t status = mpu6050_read_regs(
    mpu6050_i2c_h,
    MPU6050_REG_ACCEL_XOUT_H,
    data,
    MPU6050_BURST_SIZE
  );
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }

  float accel_g[3] = {
    (float)mpu6050_be_i16(&data[0]) / mpu6050_i2c_h->accel_lsb_per_g,
    (float)mpu6050_be_i16(&data[2]) / mpu6050_i2c_h->accel_lsb_per_g,
    (float)mpu6050_be_i16(&data[4]) / mpu6050_i2c_h->accel_lsb_per_g,
  };
  float gyro_dps[3] = {
    (float)mpu6050_be_i16(&data[8]) / mpu6050_i2c_h->gyro_lsb_per_dps,
    (float)mpu6050_be_i16(&data[10]) / mpu6050_i2c_h->gyro_lsb_per_dps,
    (float)mpu6050_be_i16(&data[12]) / mpu6050_i2c_h->gyro_lsb_per_dps,
  };

  float mapped_accel_g[3] = {0};
  float mapped_gyro_dps[3] = {0};
  mpu6050_apply_axis_map(&mpu6050_i2c_h->config, accel_g, mapped_accel_g);
  mpu6050_apply_axis_map(&mpu6050_i2c_h->config, gyro_dps, mapped_gyro_dps);

  for (uint8_t axis = 0u; axis < 3u; axis++) {
    sample->accel_mps2[axis] =
      mapped_accel_g[axis] * MPU6050_GRAVITY_MPS2;
    sample->gyro_dps[axis] = mapped_gyro_dps[axis];
  }

  int16_t temperature_raw = mpu6050_be_i16(&data[6]);
  sample->temperature_c =
    (float)temperature_raw / 340.0f + 36.53f;
  sample->time_ms = now_ms;

  return PLATFORM_STATUS_OK;
}

static bool mpu6050_accel_is_stationary(
  const mpu6050_raw_sample_t *sample
) {
  assert(sample);

  float norm_sq = 0.0f;
  for (uint8_t axis = 0u; axis < 3u; axis++) {
    norm_sq += sample->accel_mps2[axis] * sample->accel_mps2[axis];
  }

  float norm = sqrtf(norm_sq);
  return
    norm >= MPU6050_CALIBRATION_MIN_ACCEL_MPS2 &&
    norm <= MPU6050_CALIBRATION_MAX_ACCEL_MPS2;
}

static void mpu6050_calibration_reset(mpu6050_i2c_t *mpu6050_i2c_h) {
  assert(mpu6050_i2c_h);

  mpu6050_i2c_h->calibration_count = 0u;

  for (uint8_t axis = 0u; axis < 3u; axis++) {
    mpu6050_i2c_h->calibration_gyro_sum[axis] = 0.0f;
    mpu6050_i2c_h->calibration_gyro_sum_sq[axis] = 0.0f;
  }
}

static bool mpu6050_calibration_finish(mpu6050_i2c_t *mpu6050_i2c_h) {
  assert(
    mpu6050_i2c_h &&
    mpu6050_i2c_h->calibration_count == mpu6050_i2c_h->config.calibration_samples
  );

  float count = (float)mpu6050_i2c_h->calibration_count;

  for (uint8_t axis = 0u; axis < 3u; axis++) {
    float mean = mpu6050_i2c_h->calibration_gyro_sum[axis] / count;
    float variance =
      mpu6050_i2c_h->calibration_gyro_sum_sq[axis] / count - mean * mean;
    if (variance < 0.0f) {
      variance = 0.0f;
    }

    if (sqrtf(variance) > MPU6050_CALIBRATION_MAX_STD_DPS) {
      return false;
    }

    mpu6050_i2c_h->gyro_bias_dps[axis] = mean;
  }

  return true;
}

static void mpu6050_calibration_accumulate(
  mpu6050_i2c_t *mpu6050_i2c_h,
  const mpu6050_raw_sample_t *sample
) {
  assert(mpu6050_i2c_h && sample);

  for (uint8_t axis = 0u; axis < 3u; axis++) {
    mpu6050_i2c_h->calibration_gyro_sum[axis] += sample->gyro_dps[axis];
    mpu6050_i2c_h->calibration_gyro_sum_sq[axis] +=
      sample->gyro_dps[axis] * sample->gyro_dps[axis];
  }

  mpu6050_i2c_h->calibration_count++;
}

static platform_status_t mpu6050_configure(mpu6050_i2c_t *mpu6050_i2c_h) {
  assert(mpu6050_i2c_h);

  const uint8_t gyro_config =
    (uint8_t)((uint8_t)mpu6050_i2c_h->config.gyro_range << 3u);
  const uint8_t accel_config =
    (uint8_t)((uint8_t)mpu6050_i2c_h->config.accel_range << 3u);

  platform_status_t status;

  status = mpu6050_write_reg(
    mpu6050_i2c_h,
    MPU6050_REG_PWR_MGMT_1,
    MPU6050_PWR1_CLOCK_PLL_XGYRO
  );
  if (status != PLATFORM_STATUS_OK) return status;

  status = mpu6050_write_reg(
    mpu6050_i2c_h,
    MPU6050_REG_SMPLRT_DIV,
    mpu6050_i2c_h->sample_rate_divider
  );
  if (status != PLATFORM_STATUS_OK) return status;

  status = mpu6050_write_reg(
    mpu6050_i2c_h,
    MPU6050_REG_CONFIG,
    (uint8_t)mpu6050_i2c_h->config.dlpf
  );
  if (status != PLATFORM_STATUS_OK) return status;

  status = mpu6050_write_reg(
    mpu6050_i2c_h,
    MPU6050_REG_GYRO_CONFIG,
    gyro_config
  );
  if (status != PLATFORM_STATUS_OK) return status;

  status = mpu6050_write_reg(
    mpu6050_i2c_h,
    MPU6050_REG_ACCEL_CONFIG,
    accel_config
  );
  if (status != PLATFORM_STATUS_OK) return status;

  status = mpu6050_write_reg(mpu6050_i2c_h, MPU6050_REG_INT_ENABLE, 0u);
  if (status != PLATFORM_STATUS_OK) return status;

  status = mpu6050_write_reg(mpu6050_i2c_h, MPU6050_REG_USER_CTRL, 0u);
  if (status != PLATFORM_STATUS_OK) return status;

  status = mpu6050_write_reg(mpu6050_i2c_h, MPU6050_REG_PWR_MGMT_2, 0u);
  if (status != PLATFORM_STATUS_OK) return status;

  uint8_t readback = 0u;

  status = mpu6050_read_reg(mpu6050_i2c_h, MPU6050_REG_SMPLRT_DIV, &readback);
  if (status != PLATFORM_STATUS_OK) return status;
  if (readback != mpu6050_i2c_h->sample_rate_divider) return PLATFORM_STATUS_ERROR;

  status = mpu6050_read_reg(mpu6050_i2c_h, MPU6050_REG_CONFIG, &readback);
  if (status != PLATFORM_STATUS_OK) return status;
  if ((readback & 0x07u) != (uint8_t)mpu6050_i2c_h->config.dlpf) {
    return PLATFORM_STATUS_ERROR;
  }

  status = mpu6050_read_reg(mpu6050_i2c_h, MPU6050_REG_GYRO_CONFIG, &readback);
  if (status != PLATFORM_STATUS_OK) return status;
  if ((readback & 0x18u) != gyro_config) return PLATFORM_STATUS_ERROR;

  status = mpu6050_read_reg(mpu6050_i2c_h, MPU6050_REG_ACCEL_CONFIG, &readback);
  if (status != PLATFORM_STATUS_OK) return status;
  if ((readback & 0x18u) != accel_config) return PLATFORM_STATUS_ERROR;

  status = mpu6050_read_reg(mpu6050_i2c_h, MPU6050_REG_PWR_MGMT_1, &readback);
  if (status != PLATFORM_STATUS_OK) return status;
  if ((readback & 0x7Fu) != MPU6050_PWR1_CLOCK_PLL_XGYRO) {
    return PLATFORM_STATUS_ERROR;
  }

  return PLATFORM_STATUS_OK;
}

static void mpu6050_start(imu_base_t *imu_h) {
  assert(imu_h);
  mpu6050_i2c_t *mpu6050_i2c_h = (mpu6050_i2c_t *)imu_h;

  assert(!mpu6050_i2c_h->is_started);

  mpu6050_i2c_h->is_started = true;
  mpu6050_i2c_h->state = MPU6050_STATE_RESET_PENDING;
  mpu6050_i2c_h->last_request_failed = false;
}

static void mpu6050_get_status(
  imu_base_t *imu_h,
  imu_status_t *status
) {
  assert(imu_h && status);
  mpu6050_i2c_t *mpu6050_i2c_h = (mpu6050_i2c_t *)imu_h;

  uint64_t error_count =
    (uint64_t)mpu6050_i2c_h->i2c_error_count +
    (uint64_t)mpu6050_i2c_h->configuration_error_count +
    (uint64_t)mpu6050_i2c_h->calibration_error_count +
    (uint64_t)mpu6050_i2c_h->filter_reject_count;

  if (error_count > UINT32_MAX) {
    error_count = UINT32_MAX;
  }

  *status = (imu_status_t){
    .is_started = mpu6050_i2c_h->is_started,
    .is_busy =
      mpu6050_i2c_h->state == MPU6050_STATE_RESET_PENDING ||
      mpu6050_i2c_h->state == MPU6050_STATE_RESET_WAIT ||
      mpu6050_i2c_h->state == MPU6050_STATE_CALIBRATE_PENDING ||
      mpu6050_i2c_h->state == MPU6050_STATE_CALIBRATING,
    .last_request_failed = mpu6050_i2c_h->last_request_failed,
    .error_count = (uint32_t)error_count,
  };
}

static void mpu6050_set_zero(imu_base_t *imu_h) {
  assert(imu_h);
  Dev_IMU_Base_ZeroYaw(imu_h);
}

static const imu_ops_t mpu6050_imu_ops = {
  .start      = mpu6050_start,
  .process    = mpu6050_process,
  .get_status = mpu6050_get_status,
  .set_zero   = mpu6050_set_zero,
};

static bool mpu6050_config_is_valid(const mpu6050_config_t *config) {
  assert(config);

  if (
    (config->address != MPU6050_ADDRESS_AD0_LOW &&
     config->address != MPU6050_ADDRESS_AD0_HIGH) ||
    config->dlpf > MPU6050_DLPF_5HZ ||
    config->gyro_range > MPU6050_GYRO_RANGE_2000DPS ||
    config->accel_range > MPU6050_ACCEL_RANGE_16G ||
    config->sample_rate_hz < 4u ||
    config->sample_rate_hz > 1000u ||
    config->calibration_samples > 1000u ||
    config->i2c_timeout_ms == 0u
  ) {
    return false;
  }

  uint32_t output_rate = mpu6050_gyro_output_rate(config->dlpf);
  uint32_t divider =
    (output_rate + config->sample_rate_hz / 2u) /
    config->sample_rate_hz;
  divider = divider == 0u ? 0u : divider - 1u;
  if (divider > UINT8_MAX) {
    return false;
  }

  bool source_seen[3] = {false, false, false};
  for (uint8_t axis = 0u; axis < 3u; axis++) {
    uint8_t source = config->axis_source[axis];
    int8_t sign = config->axis_sign[axis];
    if (source >= 3u || source_seen[source]) {
      return false;
    }
    if (sign != -1 && sign != 1) {
      return false;
    }
    source_seen[source] = true;
  }

  return true;
}

static void mpu6050_bind_config(
  mpu6050_i2c_t *mpu6050_i2c_h,
  const mpu6050_config_t *config
) {
  assert(mpu6050_i2c_h && config);

  mpu6050_i2c_h->config = *config;
  mpu6050_i2c_h->i2c = config->i2c;
  mpu6050_i2c_h->address = config->address;
  mpu6050_i2c_h->accel_lsb_per_g =
    mpu6050_accel_lsb_per_g(config->accel_range);
  mpu6050_i2c_h->gyro_lsb_per_dps =
    mpu6050_gyro_lsb_per_dps(config->gyro_range);

  uint32_t output_rate = mpu6050_gyro_output_rate(config->dlpf);
  uint32_t divider =
    (output_rate + config->sample_rate_hz / 2u) /
    config->sample_rate_hz;
  divider = divider == 0u ? 0u : divider - 1u;
  mpu6050_i2c_h->sample_rate_divider = (uint8_t)divider;

  uint32_t period_ms =
    (1000u + config->sample_rate_hz / 2u) /
    config->sample_rate_hz;
  if (period_ms < MPU6050_SAMPLE_PERIOD_MIN_MS) {
    period_ms = MPU6050_SAMPLE_PERIOD_MIN_MS;
  }
  mpu6050_i2c_h->sample_period_ms = period_ms;
}

platform_status_t Dev_MPU6050_I2C_Init(
  mpu6050_i2c_t *imu_h,
  const char *name,
  const mpu6050_config_t *config,
  filter_base_t *filter_h
) {
  assert(imu_h && name && config && filter_h);

  if (!mpu6050_config_is_valid(config)) {
    return PLATFORM_STATUS_ERROR;
  }

  memset(imu_h, 0, sizeof(*imu_h));
  mpu6050_bind_config(imu_h, config);
  imu_h->state = MPU6050_STATE_UNINITIALIZED;

  uint8_t who_am_i = 0u;
  platform_status_t status =
    mpu6050_read_reg(imu_h, MPU6050_REG_WHO_AM_I, &who_am_i);

  if (status != PLATFORM_STATUS_OK) {
    imu_h->last_request_failed = true;
    return status;
  }

  if (
    who_am_i != MPU6050_WHO_AM_I_VALUE &&
    who_am_i != MPU6050_WHO_AM_I_FIELD
  ) {
    imu_h->last_request_failed = true;
    return PLATFORM_STATUS_ERROR;
  }

  Dev_IMU_Base_Init(
    &imu_h->imu_base,
    name,
    &mpu6050_imu_ops,
    filter_h
  );

  imu_h->last_request_failed = false;
  return PLATFORM_STATUS_OK;
}

bool Dev_MPU6050_I2C_GetRawSample(
  mpu6050_i2c_t *imu_h,
  mpu6050_raw_sample_t *sample
) {
  assert(imu_h && sample);

  Sys_Critical_Enter();
  *sample = imu_h->raw_sample;
  bool available = imu_h->has_raw_sample;
  Sys_Critical_Exit();

  return available;
}

platform_status_t Dev_MPU6050_I2C_RequestGyroCalibration(
  mpu6050_i2c_t *imu_h
) {
  assert(imu_h);

  if (
    !imu_h->is_started ||
    imu_h->config.calibration_samples == 0u ||
    imu_h->state != MPU6050_STATE_READY
  ) {
    return PLATFORM_STATUS_ERROR;
  }

  imu_h->last_request_failed = false;
  imu_h->state = MPU6050_STATE_CALIBRATE_PENDING;
  return PLATFORM_STATUS_OK;
}

static void mpu6050_process_calibration(
  mpu6050_i2c_t *mpu6050_i2c_h,
  uint32_t now_ms
) {
  assert(mpu6050_i2c_h);

  if (!mpu6050_time_reached(now_ms, mpu6050_i2c_h->next_sample_ms)) {
    return;
  }

  mpu6050_i2c_h->next_sample_ms = now_ms + mpu6050_i2c_h->sample_period_ms;

  mpu6050_raw_sample_t sample = {0};
  if (mpu6050_acquire(mpu6050_i2c_h, now_ms, &sample) != PLATFORM_STATUS_OK) {
    mpu6050_i2c_h->calibration_error_count++;
    mpu6050_enter_error(mpu6050_i2c_h);
    return;
  }

  if (!mpu6050_accel_is_stationary(&sample)) {
    mpu6050_i2c_h->calibration_error_count++;
    mpu6050_enter_error(mpu6050_i2c_h);
    return;
  }

  mpu6050_calibration_accumulate(mpu6050_i2c_h, &sample);

  if (mpu6050_i2c_h->calibration_count < mpu6050_i2c_h->config.calibration_samples) {
    return;
  }

  if (!mpu6050_calibration_finish(mpu6050_i2c_h)) {
    mpu6050_i2c_h->calibration_error_count++;
    mpu6050_enter_error(mpu6050_i2c_h);
    return;
  }

  mpu6050_i2c_h->state = MPU6050_STATE_READY;
  mpu6050_i2c_h->next_sample_ms = now_ms;
  mpu6050_i2c_h->last_request_failed = false;
}

static void mpu6050_process_ready(
  mpu6050_i2c_t *mpu6050_i2c_h,
  uint32_t now_ms
) {
  assert(mpu6050_i2c_h);

  if (!mpu6050_time_reached(now_ms, mpu6050_i2c_h->next_sample_ms)) {
    return;
  }

  mpu6050_i2c_h->next_sample_ms = now_ms + mpu6050_i2c_h->sample_period_ms;

  mpu6050_raw_sample_t raw = {0};
  if (mpu6050_acquire(mpu6050_i2c_h, now_ms, &raw) != PLATFORM_STATUS_OK) {
    mpu6050_i2c_h->last_request_failed = true;
    return;
  }

  for (uint8_t axis = 0u; axis < 3u; axis++) {
    raw.gyro_dps[axis] -= mpu6050_i2c_h->gyro_bias_dps[axis];
  }

  mpu6050_store_raw(mpu6050_i2c_h, &raw);

  filter_sample_t input = {
    .valid_mask =
      IMU_CHANNEL_ACCEL_X |
      IMU_CHANNEL_ACCEL_Y |
      IMU_CHANNEL_ACCEL_Z |
      IMU_CHANNEL_GYRO_X |
      IMU_CHANNEL_GYRO_Y |
      IMU_CHANNEL_GYRO_Z,
    .time_ms = now_ms,
  };

  input.value[IMU_CHANNEL_INDEX_ACCEL_X] = raw.accel_mps2[0];
  input.value[IMU_CHANNEL_INDEX_ACCEL_Y] = raw.accel_mps2[1];
  input.value[IMU_CHANNEL_INDEX_ACCEL_Z] = raw.accel_mps2[2];
  input.value[IMU_CHANNEL_INDEX_GYRO_X] = raw.gyro_dps[0];
  input.value[IMU_CHANNEL_INDEX_GYRO_Y] = raw.gyro_dps[1];
  input.value[IMU_CHANNEL_INDEX_GYRO_Z] = raw.gyro_dps[2];

  if (Dev_IMU_Base_SubmitSample(&mpu6050_i2c_h->imu_base, &input)) {
    mpu6050_i2c_h->last_request_failed = false;
  } else {
    mpu6050_i2c_h->filter_reject_count++;
    mpu6050_i2c_h->last_request_failed = true;
  }
}

static void mpu6050_process(imu_base_t *imu_h, uint32_t now_ms) {
  assert(imu_h);
  mpu6050_i2c_t *mpu6050_i2c_h = (mpu6050_i2c_t *)imu_h;

  if (!mpu6050_i2c_h->is_started) {
    return;
  }

  switch (mpu6050_i2c_h->state) {
    case MPU6050_STATE_RESET_PENDING:
      if (
        mpu6050_write_reg(
          mpu6050_i2c_h,
          MPU6050_REG_PWR_MGMT_1,
          MPU6050_PWR1_DEVICE_RESET
        ) != PLATFORM_STATUS_OK
      ) {
        mpu6050_enter_error(mpu6050_i2c_h);
        return;
      }
      mpu6050_i2c_h->state = MPU6050_STATE_RESET_WAIT;
      mpu6050_i2c_h->state_deadline_ms = now_ms + MPU6050_RESET_DELAY_MS;
      break;

    case MPU6050_STATE_RESET_WAIT:
      if (!mpu6050_time_reached(now_ms, mpu6050_i2c_h->state_deadline_ms)) {
        return;
      }
      if (mpu6050_configure(mpu6050_i2c_h) != PLATFORM_STATUS_OK) {
        mpu6050_i2c_h->configuration_error_count++;
        mpu6050_i2c_h->last_request_failed = true;
        mpu6050_enter_error(mpu6050_i2c_h);
        return;
      }
      mpu6050_i2c_h->state = MPU6050_STATE_CALIBRATE_PENDING;
      break;

    case MPU6050_STATE_CALIBRATE_PENDING:
      mpu6050_calibration_reset(mpu6050_i2c_h);
      mpu6050_i2c_h->next_sample_ms = now_ms;
      if (mpu6050_i2c_h->config.calibration_samples == 0u) {
        for (uint8_t axis = 0u; axis < 3u; axis++) {
          mpu6050_i2c_h->gyro_bias_dps[axis] = 0.0f;
        }
        mpu6050_i2c_h->state = MPU6050_STATE_READY;
      } else {
        mpu6050_i2c_h->state = MPU6050_STATE_CALIBRATING;
      }
      break;

    case MPU6050_STATE_CALIBRATING:
      mpu6050_process_calibration(mpu6050_i2c_h, now_ms);
      break;

    case MPU6050_STATE_READY:
      mpu6050_process_ready(mpu6050_i2c_h, now_ms);
      break;

    case MPU6050_STATE_UNINITIALIZED:
    case MPU6050_STATE_ERROR:
    default:
      return;
  }
}
