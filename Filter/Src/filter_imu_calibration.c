//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file filter_imu_calibration.c
 * @brief 统一 IMU 六轴校正滤波器实现。
 * @ingroup filter
 */

#include "filter_imu_calibration.h"
#include "system_critical.h"
#include "system_math.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

#define FILTER_IMU_CAL_GYRO_X_CHANNEL   3u
#define FILTER_IMU_CAL_GYRO_Y_CHANNEL   4u
#define FILTER_IMU_CAL_GYRO_Z_CHANNEL   5u
#define FILTER_IMU_CAL_ACCEL_X_CHANNEL  6u
#define FILTER_IMU_CAL_ACCEL_Y_CHANNEL  7u
#define FILTER_IMU_CAL_ACCEL_Z_CHANNEL  8u
#define FILTER_IMU_CAL_SCALE_MIN        0.1f
#define FILTER_IMU_CAL_SCALE_MAX        10.0f
#define FILTER_IMU_CAL_GYRO_BIAS_MAX_DPS 720.0f

static bool filter_imucalibration_configisvalid(
  const filter_imu_calibration_config_t *config
) {
  assert(config);

  for (uint8_t axis = 0u; axis < 3u; axis++) {
    if (
      !SYS_ISFINITE(config->accel_offset_mps2[axis]) ||
      !SYS_ISFINITE(config->accel_scale[axis]) ||
      !SYS_ISFINITE(config->gyro_bias_dps[axis]) ||
      config->accel_scale[axis] < FILTER_IMU_CAL_SCALE_MIN ||
      config->accel_scale[axis] > FILTER_IMU_CAL_SCALE_MAX ||
      SYS_ABS(config->gyro_bias_dps[axis]) > FILTER_IMU_CAL_GYRO_BIAS_MAX_DPS
    ) {
      return false;
    }
  }

  return true;
}

static float filter_imucalibration_apply(
  float value,
  float offset,
  float scale
) {
  return (value - offset) * scale;
}

static void filter_imucalibration_reset(filter_base_t *filter_h) {
  assert(filter_h);
  filter_imu_calibration_t *imu_calibration_h =
    (filter_imu_calibration_t *)filter_h;
  imu_calibration_h->corrected_frames = 0u;
}

static void filter_imucalibration_update(
  filter_base_t *filter_h,
  const filter_sample_t *input,
  filter_sample_t *output
) {
  assert(filter_h && input && output);

  filter_imu_calibration_t *imu_calibration_h =
    (filter_imu_calibration_t *)filter_h;
  filter_imu_calibration_config_t config;

  Sys_Critical_Enter();
  config = imu_calibration_h->config;
  Sys_Critical_Exit();

  const uint32_t gyro_channels[3] = {
    FILTER_IMU_CAL_GYRO_X_CHANNEL,
    FILTER_IMU_CAL_GYRO_Y_CHANNEL,
    FILTER_IMU_CAL_GYRO_Z_CHANNEL,
  };
  const uint32_t accel_channels[3] = {
    FILTER_IMU_CAL_ACCEL_X_CHANNEL,
    FILTER_IMU_CAL_ACCEL_Y_CHANNEL,
    FILTER_IMU_CAL_ACCEL_Z_CHANNEL,
  };

  for (uint8_t axis = 0u; axis < 3u; axis++) {
    uint32_t gyro_mask = (uint32_t)1u << gyro_channels[axis];
    if ((output->valid_mask & gyro_mask) != 0u) {
      float corrected =
        output->value[gyro_channels[axis]] -
        config.gyro_bias_dps[axis];
      if (SYS_ISFINITE(corrected)) {
        output->value[gyro_channels[axis]] = corrected;
      } else {
        output->valid_mask &= ~gyro_mask;
      }
    }

    uint32_t accel_mask = (uint32_t)1u << accel_channels[axis];
    if ((output->valid_mask & accel_mask) != 0u) {
      float corrected = filter_imucalibration_apply(
        output->value[accel_channels[axis]],
        config.accel_offset_mps2[axis],
        config.accel_scale[axis]
      );
      if (SYS_ISFINITE(corrected)) {
        output->value[accel_channels[axis]] = corrected;
      } else {
        output->valid_mask &= ~accel_mask;
      }
    }
  }

  imu_calibration_h->corrected_frames++;
}

static const filter_ops_t filter_ops_imu_calibration = {
  .update = filter_imucalibration_update,
  .reset = filter_imucalibration_reset,
};

bool Filter_ImuCalibration_Init(
  filter_imu_calibration_t *filter_h,
  const char *name,
  const filter_imu_calibration_config_t *config
) {
  if (
    filter_h == NULL ||
    name == NULL ||
    config == NULL ||
    !filter_imucalibration_configisvalid(config)
  ) {
    return false;
  }

  memset(filter_h, 0, sizeof(*filter_h));
  Filter_Base_Init(
    &filter_h->filter_base,
    name,
    &filter_ops_imu_calibration
  );
  filter_h->config = *config;
  Filter_Base_Reset(&filter_h->filter_base);
  return true;
}

bool Filter_ImuCalibration_SetConfig(
  filter_imu_calibration_t *filter_h,
  const filter_imu_calibration_config_t *config
) {
  if (
    filter_h == NULL ||
    config == NULL ||
    !filter_imucalibration_configisvalid(config)
  ) {
    return false;
  }

  Sys_Critical_Enter();
  filter_h->config = *config;
  Sys_Critical_Exit();
  return true;
}

void Filter_ImuCalibration_GetConfig(
  const filter_imu_calibration_t *filter_h,
  filter_imu_calibration_config_t *config
) {
  assert(filter_h && config);

  Sys_Critical_Enter();
  *config = filter_h->config;
  Sys_Critical_Exit();
}
