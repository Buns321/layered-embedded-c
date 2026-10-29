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

/**
 * @brief 校验 IMU 校正配置。
 */
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

/**
 * @brief 把校正配置应用到 IMU 校正滤波器。
 */
static float filter_imucalibration_apply(
  float value,
  float offset,
  float scale
) {
  return (value - offset) * scale;
}

/**
 * @brief 重置 IMU 校正内部状态。
 */
static void filter_imucalibration_reset(filter_base_t *filter_h) {
  assert(filter_h);
  filter_imu_calibration_t *imu_calibration_h =
    (filter_imu_calibration_t *)filter_h;
  imu_calibration_h->corrected_frames = 0u;
}

/**
 * @brief 应用偏置、比例和安装变换。
 */
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

/**
 * @brief 初始化六轴校正滤波器。
 * @param filter_h 对象。
 * @param name 名称。
 * @param config 初始校正参数，不能为空。
 * @retval true 初始化和参数校验成功。
 * @retval false 参数为空、非有限或比例系数超出安全范围。
 * @note 只允许在任务初始化阶段调用。
 */
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

/**
 * @brief 在帧边界更新六轴校正参数。
 * @param filter_h 对象。
 * @param config 新参数。
 * @retval true 参数有效且已提交。
 * @retval false 参数无效，原配置保持不变。
 * @note 可以从另一任务调用；配置只在一次完整 update 前复制并应用。
 * @pre filter_h 和 config 非空。
 */
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

/**
 * @brief 复制当前六轴校正参数。
 * @param filter_h 对象。
 * @param config 输出参数。
 * @pre filter_h 和 config 非空。
 */
void Filter_ImuCalibration_GetConfig(
  const filter_imu_calibration_t *filter_h,
  filter_imu_calibration_config_t *config
) {
  assert(filter_h && config);

  Sys_Critical_Enter();
  *config = filter_h->config;
  Sys_Critical_Exit();
}
