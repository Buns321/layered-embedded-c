//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file filter_ekf.c
 * @brief 七状态四元数 EKF 的接口壳层与占位核心。
 * @ingroup filter
 */

#include "filter_ekf.h"
#include "system_critical.h"
#include "system_math.h"
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#define FILTER_EKF_DEG_TO_RAD 0.01745329251994329577f
#define FILTER_EKF_RAD_TO_DEG 57.295779513082320876f
#define FILTER_EKF_IMU_YAW_CHANNEL   0u
#define FILTER_EKF_IMU_ROLL_CHANNEL  1u
#define FILTER_EKF_IMU_PITCH_CHANNEL 2u
#define FILTER_EKF_IMU_GYRO_X_CHANNEL 3u
#define FILTER_EKF_IMU_GYRO_Y_CHANNEL 4u
#define FILTER_EKF_IMU_GYRO_Z_CHANNEL 5u
#define FILTER_EKF_IMU_ACCEL_X_CHANNEL 6u
#define FILTER_EKF_IMU_ACCEL_Y_CHANNEL 7u
#define FILTER_EKF_IMU_ACCEL_Z_CHANNEL 8u
#define FILTER_EKF_ALL_PARAMETER_MASK \
  ((uint32_t)((1u << FILTER_EKF_PARAM_COUNT) - 1u))

static bool filter_ekf_valueispositivefinite(float value) {
  return SYS_ISFINITE(value) && value > 0.0f;
}

static bool filter_ekf_configisvalid(const filter_ekf_config_t *config) {
  if (config == NULL) {
    return false;
  }

  return
    filter_ekf_valueispositivefinite(config->angle_process_noise_rad2) &&
    filter_ekf_valueispositivefinite(config->bias_process_noise_radps2) &&
    filter_ekf_valueispositivefinite(
      config->accel_measurement_noise_mps2_2
    ) &&
    filter_ekf_valueispositivefinite(
      config->initial_angle_variance_rad2
    ) &&
    filter_ekf_valueispositivefinite(
      config->initial_bias_variance_radps2
    ) &&
    SYS_ISFINITE(config->accel_norm_min_mps2) &&
    SYS_ISFINITE(config->accel_norm_max_mps2) &&
    config->accel_norm_min_mps2 > 0.0f &&
    config->accel_norm_max_mps2 > config->accel_norm_min_mps2 &&
    filter_ekf_valueispositivefinite(config->gyro_limit_radps) &&
    filter_ekf_valueispositivefinite(config->bias_limit_radps) &&
    filter_ekf_valueispositivefinite(config->min_dt_s) &&
    filter_ekf_valueispositivefinite(config->max_dt_s) &&
    config->max_dt_s >= config->min_dt_s;
}

static float *filter_ekf_configfield(
  filter_ekf_config_t *config,
  filter_ekf_parameter_t parameter
) {
  assert(config);

  switch (parameter) {
    case FILTER_EKF_PARAM_ANGLE_PROCESS_NOISE:
      return &config->angle_process_noise_rad2;
    case FILTER_EKF_PARAM_BIAS_PROCESS_NOISE:
      return &config->bias_process_noise_radps2;
    case FILTER_EKF_PARAM_ACCEL_MEASUREMENT_NOISE:
      return &config->accel_measurement_noise_mps2_2;
    case FILTER_EKF_PARAM_INITIAL_ANGLE_VARIANCE:
      return &config->initial_angle_variance_rad2;
    case FILTER_EKF_PARAM_INITIAL_BIAS_VARIANCE:
      return &config->initial_bias_variance_radps2;
    case FILTER_EKF_PARAM_ACCEL_NORM_MIN:
      return &config->accel_norm_min_mps2;
    case FILTER_EKF_PARAM_ACCEL_NORM_MAX:
      return &config->accel_norm_max_mps2;
    case FILTER_EKF_PARAM_GYRO_LIMIT:
      return &config->gyro_limit_radps;
    case FILTER_EKF_PARAM_BIAS_LIMIT:
      return &config->bias_limit_radps;
    case FILTER_EKF_PARAM_MIN_DT:
      return &config->min_dt_s;
    case FILTER_EKF_PARAM_MAX_DT:
      return &config->max_dt_s;
    default:
      return NULL;
  }
}

static void filter_ekf_initializecovariance(filter_ekf_t *filter_h) {
  assert(filter_h);

  memset(
    filter_h->covariance,
    0,
    sizeof(filter_h->covariance)
  );

  for (uint8_t index = 0u; index < 4u; index++) {
    filter_h->covariance[index][index] =
      filter_h->config.initial_angle_variance_rad2;
  }
  for (uint8_t index = 4u; index < FILTER_EKF_STATE_DIMENSION; index++) {
    filter_h->covariance[index][index] =
      filter_h->config.initial_bias_variance_radps2;
  }
}

static void filter_ekf_applypendingconfig(filter_ekf_t *filter_h) {
  assert(filter_h);

  filter_ekf_config_t pending_config;
  uint32_t pending_mask;

  Sys_Critical_Enter();
  pending_config = filter_h->pending_config;
  pending_mask = filter_h->pending_mask;
  filter_h->pending_mask = 0u;
  Sys_Critical_Exit();

  if (pending_mask == 0u) {
    return;
  }

  if (filter_ekf_configisvalid(&pending_config)) {
    filter_h->config = pending_config;
  } else {
    filter_h->pending_config = filter_h->config;
  }
}

static float filter_ekf_clampdeltatime(
  filter_ekf_t *filter_h,
  uint32_t now_ms
) {
  assert(filter_h);

  float dt_s = filter_h->config.min_dt_s;

  if (filter_h->has_time) {
    uint32_t elapsed_ms = now_ms - filter_h->last_time_ms;
    dt_s = (float)elapsed_ms * 0.001f;
    dt_s = SYS_CLAMP(
      dt_s,
      filter_h->config.min_dt_s,
      filter_h->config.max_dt_s
    );
  }

  filter_h->last_time_ms = now_ms;
  filter_h->has_time = true;
  return dt_s;
}

static bool filter_ekf_inputframeisusable(const filter_sample_t *input) {
  assert(input);
  const uint32_t required_mask =
    (uint32_t)(1u << FILTER_EKF_IMU_GYRO_X_CHANNEL) |
    (uint32_t)(1u << FILTER_EKF_IMU_GYRO_Y_CHANNEL) |
    (uint32_t)(1u << FILTER_EKF_IMU_GYRO_Z_CHANNEL) |
    (uint32_t)(1u << FILTER_EKF_IMU_ACCEL_X_CHANNEL) |
    (uint32_t)(1u << FILTER_EKF_IMU_ACCEL_Y_CHANNEL) |
    (uint32_t)(1u << FILTER_EKF_IMU_ACCEL_Z_CHANNEL);
  return (input->valid_mask & required_mask) == required_mask;
}

static void filter_ekf_coreupdate(
  filter_ekf_t *filter_h,
  const float gyro_radps[3],
  const float accel_mps2[3],
  float dt_s
) {
  assert(filter_h && gyro_radps && accel_mps2);
  (void)gyro_radps;
  (void)accel_mps2;
  (void)dt_s;

  // EKF 预测与校正由使用者在此实现；此阶段只发布确定的单位四元数占位值。
  filter_h->state.quaternion[0] = 1.0f;
  filter_h->state.quaternion[1] = 0.0f;
  filter_h->state.quaternion[2] = 0.0f;
  filter_h->state.quaternion[3] = 0.0f;
  filter_h->state.roll_rad = 0.0f;
  filter_h->state.pitch_rad = 0.0f;
  filter_h->state.yaw_rad = 0.0f;
  filter_h->state.placeholder = true;
  filter_h->state.valid = true;
}

static void filter_ekf_publishattitude(
  filter_ekf_t *filter_h,
  filter_sample_t *output
) {
  assert(filter_h && output);

  const uint32_t yaw_mask =
    (uint32_t)1u << FILTER_EKF_IMU_YAW_CHANNEL;
  const uint32_t roll_mask =
    (uint32_t)1u << FILTER_EKF_IMU_ROLL_CHANNEL;
  const uint32_t pitch_mask =
    (uint32_t)1u << FILTER_EKF_IMU_PITCH_CHANNEL;

  output->value[FILTER_EKF_IMU_YAW_CHANNEL] =
    filter_h->state.yaw_rad * FILTER_EKF_RAD_TO_DEG;
  output->value[FILTER_EKF_IMU_ROLL_CHANNEL] =
    filter_h->state.roll_rad * FILTER_EKF_RAD_TO_DEG;
  output->value[FILTER_EKF_IMU_PITCH_CHANNEL] =
    filter_h->state.pitch_rad * FILTER_EKF_RAD_TO_DEG;

  if (filter_h->state.valid) {
    output->valid_mask |= yaw_mask | roll_mask | pitch_mask;
  } else {
    output->valid_mask &= ~(yaw_mask | roll_mask | pitch_mask);
  }
}

static void filter_ekf_reset(filter_base_t *filter_h) {
  assert(filter_h);
  filter_ekf_t *filter_ekf_h = (filter_ekf_t *)filter_h;

  filter_ekf_h->state = (filter_ekf_state_t){
    .quaternion = {1.0f, 0.0f, 0.0f, 0.0f},
    .placeholder = true,
  };
  filter_ekf_h->last_time_ms = 0u;
  filter_ekf_h->has_time = false;
  filter_ekf_initializecovariance(filter_ekf_h);
}

static void filter_ekf_update(
  filter_base_t *filter_h,
  const filter_sample_t *input,
  filter_sample_t *output
) {
  assert(filter_h && input && output);
  filter_ekf_t *filter_ekf_h = (filter_ekf_t *)filter_h;

  filter_ekf_applypendingconfig(filter_ekf_h);
  float dt_s = filter_ekf_clampdeltatime(filter_ekf_h, input->time_ms);

  if (!filter_ekf_inputframeisusable(input)) {
    filter_ekf_h->state.valid = false;
    filter_ekf_h->state.dt_s = dt_s;
    filter_ekf_publishattitude(filter_ekf_h, output);
    return;
  }

  float gyro_radps[3] = {
    input->value[FILTER_EKF_IMU_GYRO_X_CHANNEL] *
      FILTER_EKF_DEG_TO_RAD,
    input->value[FILTER_EKF_IMU_GYRO_Y_CHANNEL] *
      FILTER_EKF_DEG_TO_RAD,
    input->value[FILTER_EKF_IMU_GYRO_Z_CHANNEL] *
      FILTER_EKF_DEG_TO_RAD,
  };
  float accel_mps2[3] = {
    input->value[FILTER_EKF_IMU_ACCEL_X_CHANNEL],
    input->value[FILTER_EKF_IMU_ACCEL_Y_CHANNEL],
    input->value[FILTER_EKF_IMU_ACCEL_Z_CHANNEL],
  };

  filter_ekf_coreupdate(
    filter_ekf_h,
    gyro_radps,
    accel_mps2,
    dt_s
  );
  filter_ekf_h->state.dt_s = dt_s;
  filter_ekf_h->state.update_count++;
  filter_ekf_publishattitude(filter_ekf_h, output);
}

static const filter_ops_t filter_ops_ekf = {
  .update = filter_ekf_update,
  .reset = filter_ekf_reset,
};

bool Filter_Ekf_Init(
  filter_ekf_t *filter_h,
  const char *name,
  const filter_ekf_config_t *config
) {
  if (
    filter_h == NULL ||
    name == NULL ||
    !filter_ekf_configisvalid(config)
  ) {
    return false;
  }

  memset(filter_h, 0, sizeof(*filter_h));
  Filter_Base_Init(&filter_h->filter_base, name, &filter_ops_ekf);
  filter_h->config = *config;
  filter_h->default_config = *config;
  filter_h->pending_config = *config;
  filter_h->pending_mask = 0u;
  Filter_Base_Reset(&filter_h->filter_base);
  return true;
}

bool Filter_Ekf_SetParameter(
  filter_ekf_t *filter_h,
  filter_ekf_parameter_t parameter,
  float value
) {
  if (
    filter_h == NULL ||
    (uint32_t)parameter >= (uint32_t)FILTER_EKF_PARAM_COUNT ||
    !SYS_ISFINITE(value)
  ) {
    return false;
  }

  filter_ekf_config_t candidate;
  uint32_t pending_mask;

  Sys_Critical_Enter();
  candidate = filter_h->pending_mask != 0u
    ? filter_h->pending_config
    : filter_h->config;
  pending_mask = filter_h->pending_mask;
  Sys_Critical_Exit();

  float *field = filter_ekf_configfield(&candidate, parameter);
  if (field == NULL) {
    return false;
  }
  *field = value;

  if (!filter_ekf_configisvalid(&candidate)) {
    return false;
  }

  Sys_Critical_Enter();
  filter_h->pending_config = candidate;
  filter_h->pending_mask = pending_mask | ((uint32_t)1u << parameter);
  Sys_Critical_Exit();
  return true;
}

void Filter_Ekf_ResetParameters(filter_ekf_t *filter_h) {
  assert(filter_h);

  Sys_Critical_Enter();
  filter_h->pending_config = filter_h->default_config;
  filter_h->pending_mask = FILTER_EKF_ALL_PARAMETER_MASK;
  Sys_Critical_Exit();
}

void Filter_Ekf_GetConfig(
  const filter_ekf_t *filter_h,
  filter_ekf_config_t *config
) {
  assert(filter_h && config);

  Sys_Critical_Enter();
  *config = filter_h->config;
  Sys_Critical_Exit();
}

void Filter_Ekf_GetState(
  const filter_ekf_t *filter_h,
  filter_ekf_state_t *state
) {
  assert(filter_h && state);

  Sys_Critical_Enter();
  *state = filter_h->state;
  Sys_Critical_Exit();
}
