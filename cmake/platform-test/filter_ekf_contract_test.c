/**
 * @file filter_ekf_contract_test.c
 * @brief 七状态 EKF 壳层与在线参数契约测试。
 * @ingroup tests
 */

#include "filter_base.h"
#include "filter_ekf.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

void Sys_Critical_Enter(void) {
}

void Sys_Critical_Exit(void) {
}

static const filter_ekf_config_t s_default_config = {
  .angle_process_noise_rad2 = 1.0e-4f,
  .bias_process_noise_radps2 = 1.0e-6f,
  .accel_measurement_noise_mps2_2 = 0.1f,
  .initial_angle_variance_rad2 = 1.0f,
  .initial_bias_variance_radps2 = 0.01f,
  .accel_norm_min_mps2 = 7.8f,
  .accel_norm_max_mps2 = 11.8f,
  .gyro_limit_radps = 17.45f,
  .bias_limit_radps = 1.0f,
  .min_dt_s = 0.001f,
  .max_dt_s = 0.05f,
};

static bool floats_near(float left, float right, float tolerance) {
  return fabsf(left - right) <= tolerance;
}

static filter_sample_t make_complete_input(uint32_t time_ms) {
  filter_sample_t input = {
    .valid_mask =
      (1u << 3) | (1u << 4) | (1u << 5) |
      (1u << 6) | (1u << 7) | (1u << 8),
    .time_ms = time_ms,
  };
  input.value[8] = 9.80665f;
  return input;
}

static void test_placeholder_and_time(void) {
  filter_ekf_t ekf;
  assert(Filter_Ekf_Init(&ekf, "ekf", &s_default_config));

  filter_sample_t input = make_complete_input(100u);
  filter_sample_t output;
  Filter_Base_Update(&ekf.filter_base, &input, &output);
  assert((output.valid_mask & 0x07u) == 0x07u);
  assert(floats_near(output.value[0], 0.0f, 1e-6f));
  assert(floats_near(output.value[1], 0.0f, 1e-6f));
  assert(floats_near(output.value[2], 0.0f, 1e-6f));

  filter_ekf_state_t state;
  Filter_Ekf_GetState(&ekf, &state);
  assert(state.valid);
  assert(state.placeholder);
  assert(state.update_count == 1u);
  assert(floats_near(state.dt_s, s_default_config.min_dt_s, 1e-6f));
  assert(floats_near(state.quaternion[0], 1.0f, 1e-6f));

  input.time_ms = 120u;
  Filter_Base_Update(&ekf.filter_base, &input, &output);
  Filter_Ekf_GetState(&ekf, &state);
  assert(floats_near(state.dt_s, 0.02f, 1e-6f));
}

static void test_incomplete_input(void) {
  filter_ekf_t ekf;
  assert(Filter_Ekf_Init(&ekf, "ekf", &s_default_config));

  filter_sample_t input = {
    .valid_mask = (1u << 5) | (1u << 8),
    .time_ms = 5u,
  };
  filter_sample_t output;
  Filter_Base_Update(&ekf.filter_base, &input, &output);
  assert((output.valid_mask & 0x07u) == 0u);

  filter_ekf_state_t state;
  Filter_Ekf_GetState(&ekf, &state);
  assert(!state.valid);
}

static void test_online_parameters(void) {
  filter_ekf_t ekf;
  assert(Filter_Ekf_Init(&ekf, "ekf", &s_default_config));

  filter_ekf_config_t invalid_config = s_default_config;
  invalid_config.max_dt_s = invalid_config.min_dt_s * 0.5f;
  assert(!Filter_Ekf_Init(&ekf, "ekf", &invalid_config));

  assert(Filter_Ekf_SetParameter(
    &ekf,
    FILTER_EKF_PARAM_ACCEL_MEASUREMENT_NOISE,
    0.2f
  ));
  assert(!Filter_Ekf_SetParameter(
    &ekf,
    FILTER_EKF_PARAM_COUNT,
    1.0f
  ));
  assert(!Filter_Ekf_SetParameter(
    &ekf,
    FILTER_EKF_PARAM_ACCEL_MEASUREMENT_NOISE,
    0.0f
  ));
  assert(!Filter_Ekf_SetParameter(
    &ekf,
    FILTER_EKF_PARAM_ACCEL_MEASUREMENT_NOISE,
    NAN
  ));
  assert(!Filter_Ekf_SetParameter(
    &ekf,
    FILTER_EKF_PARAM_ACCEL_NORM_MIN,
    12.0f
  ));

  filter_ekf_config_t before;
  Filter_Ekf_GetConfig(&ekf, &before);
  assert(floats_near(
    before.accel_measurement_noise_mps2_2,
    0.1f,
    1e-6f
  ));

  filter_sample_t input = make_complete_input(10u);
  filter_sample_t output;
  Filter_Base_Update(&ekf.filter_base, &input, &output);

  filter_ekf_config_t after;
  Filter_Ekf_GetConfig(&ekf, &after);
  assert(floats_near(
    after.accel_measurement_noise_mps2_2,
    0.2f,
    1e-6f
  ));

  Filter_Ekf_ResetParameters(&ekf);
  input.time_ms = 20u;
  Filter_Base_Update(&ekf.filter_base, &input, &output);
  Filter_Ekf_GetConfig(&ekf, &after);
  assert(floats_near(
    after.accel_measurement_noise_mps2_2,
    s_default_config.accel_measurement_noise_mps2_2,
    1e-6f
  ));
}

int main(void) {
  test_placeholder_and_time();
  test_incomplete_input();
  test_online_parameters();
  return 0;
}
