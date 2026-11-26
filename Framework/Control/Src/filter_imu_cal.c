/**
 * @file filter_imu_cal.c
 * @brief IMU 六轴零偏与比例校正滤波器实现。
 * @ingroup control
 */

#include "filter_imu_cal.h"
#include "sys_math.h"
#include "sys_snapshot.h"
#include <stdbool.h>
#include <stddef.h>

#define FILTER_IMU_CAL_GYRO_X_CH 3U
#define FILTER_IMU_CAL_GYRO_Y_CH 4U
#define FILTER_IMU_CAL_GYRO_Z_CH 5U
#define FILTER_IMU_CAL_ACCEL_X_CH 6U
#define FILTER_IMU_CAL_ACCEL_Y_CH 7U
#define FILTER_IMU_CAL_ACCEL_Z_CH 8U
#define FILTER_IMU_CAL_SCALE_MIN 0.1f
#define FILTER_IMU_CAL_SCALE_MAX 10.0f
#define FILTER_IMU_CAL_GYRO_BIAS_MAX_DPS 720.0f

static bool filter_imucal_cfgisvalid(const filter_imu_cal_cfg_t *cfg) {
  for (uint8_t axis = 0U; axis < 3U; ++axis) {
    if (!SYS_ISFINITE(cfg->accel_offset_mps2[axis]) || !SYS_ISFINITE(cfg->accel_scale[axis]) ||
        !SYS_ISFINITE(cfg->gyro_bias_dps[axis]) ||
        cfg->accel_scale[axis] < FILTER_IMU_CAL_SCALE_MIN ||
        cfg->accel_scale[axis] > FILTER_IMU_CAL_SCALE_MAX ||
        SYS_ABS(cfg->gyro_bias_dps[axis]) > FILTER_IMU_CAL_GYRO_BIAS_MAX_DPS) {
      return false;
    }
  }
  return true;
}

static filter_status_t filter_imucal_init(filter_base_t *filter_h,
                                          const filter_config_t *filter_config) {
  filter_imu_cal_t *cal = (filter_imu_cal_t *)filter_h;
  const filter_imu_cal_cfg_t *cfg = (const filter_imu_cal_cfg_t *)filter_config->data;

  if (cfg == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }
  if (!filter_imucal_cfgisvalid(cfg)) {
    return FILTER_STATUS_INVALID_CONFIG;
  }

  cal->cfg = *cfg;
  return FILTER_STATUS_OK;
}

static void filter_imucal_reset(filter_base_t *filter_h) {
  filter_imu_cal_t *cal = (filter_imu_cal_t *)filter_h;
  cal->corrected_frames = 0U;
}

static void filter_imucal_update(filter_base_t *filter_h, filter_sample_t *sample) {
  static const uint8_t s_gyro_ch[3] = {
      FILTER_IMU_CAL_GYRO_X_CH,
      FILTER_IMU_CAL_GYRO_Y_CH,
      FILTER_IMU_CAL_GYRO_Z_CH,
  };
  static const uint8_t s_accel_ch[3] = {
      FILTER_IMU_CAL_ACCEL_X_CH,
      FILTER_IMU_CAL_ACCEL_Y_CH,
      FILTER_IMU_CAL_ACCEL_Z_CH,
  };

  filter_imu_cal_t *cal = (filter_imu_cal_t *)filter_h;
  filter_imu_cal_cfg_t cfg;
  SYS_SNAPSHOT_READ(&cfg, &cal->cfg);

  for (uint8_t axis = 0U; axis < 3U; ++axis) {
    uint8_t g_ch = s_gyro_ch[axis];
    if ((sample->valid_mask & Filter_Sample_GetChannelMask(g_ch)) != 0U) {
      float corrected = sample->value[g_ch] - cfg.gyro_bias_dps[axis];
      (void)Filter_Sample_WriteFiniteChannel(sample, g_ch, corrected);
    }

    uint8_t a_ch = s_accel_ch[axis];
    if ((sample->valid_mask & Filter_Sample_GetChannelMask(a_ch)) != 0U) {
      float corrected = (sample->value[a_ch] - cfg.accel_offset_mps2[axis]) * cfg.accel_scale[axis];
      (void)Filter_Sample_WriteFiniteChannel(sample, a_ch, corrected);
    }
  }

  cal->corrected_frames++;
}

static const filter_ops_t s_filter_ops_imu_cal = {
    .init = filter_imucal_init,
    .update = filter_imucal_update,
    .reset = filter_imucal_reset,
};

filter_status_t Filter_ImuCal_Init(filter_imu_cal_t *filter_h, const char *name,
                                   const filter_imu_cal_cfg_t *cfg) {
  if (filter_h == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }
  const filter_config_t config = {
      .name = name,
      .data = cfg,
  };
  return Filter_Base_Construct(&filter_h->base, &config, &s_filter_ops_imu_cal);
}

filter_status_t Filter_ImuCal_SetConfig(filter_imu_cal_t *filter_h,
                                        const filter_imu_cal_cfg_t *cfg) {
  if (filter_h == NULL || cfg == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }
  if (!filter_imucal_cfgisvalid(cfg)) {
    return FILTER_STATUS_INVALID_CONFIG;
  }
  SYS_SNAPSHOT_WRITE(&filter_h->cfg, cfg);
  return FILTER_STATUS_OK;
}

void Filter_ImuCal_GetConfig(const filter_imu_cal_t *filter_h, filter_imu_cal_cfg_t *cfg) {
  if (filter_h == NULL || cfg == NULL) {
    return;
  }
  SYS_SNAPSHOT_READ(cfg, &filter_h->cfg);
}
