/**
 * @file test_filters.c
 * @brief filter_base / passthrough / lowpass / chain / imu_cal 宿主机单元测试。
 * @ingroup tests
 */

#include "filter_chain.h"
#include "filter_imu_cal.h"
#include "filter_lowpass.h"
#include "filter_passthrough.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static bool nearf(float a, float b, float eps) { return fabsf(a - b) <= eps; }

static void test_passthrough_and_sanitize(void) {
  filter_passthrough_t pt;
  assert(Filter_Passthrough_Init(&pt, NULL) == FILTER_STATUS_INVALID_ARGUMENT);
  assert(Filter_Passthrough_Init(&pt, "pt") == FILTER_STATUS_OK);

  filter_sample_t in = {
      .valid_mask = 0x80000003U,
      .time_ms = 1234U,
  };
  in.value[0] = 42.5f;
  in.value[1] = NAN;

  filter_sample_t out = {0};
  Filter_Base_Update(&pt.base, &in, &out);
  assert(out.valid_mask == 0x00000001U);
  assert(nearf(out.value[0], 42.5f, 1e-6f));
  assert(out.time_ms == 1234U);
}

static void test_lowpass_and_wraparound(void) {
  filter_lowpass_t lp;
  assert(Filter_Lowpass_Init(&lp, "lp", 0.0f) == FILTER_STATUS_INVALID_CONFIG);
  assert(Filter_Lowpass_Init(&lp, "lp", 0.09f) == FILTER_STATUS_OK);
  assert(Filter_Lowpass_SetChannelTimeConstant(&lp, 1U, 0.01f) == FILTER_STATUS_OK);
  assert(Filter_Lowpass_SetChannelTimeConstant(&lp, FILTER_CHANNEL_CAPACITY, 0.1f) ==
         FILTER_STATUS_INVALID_CONFIG);

  filter_sample_t in = {.valid_mask = 0x3U, .time_ms = UINT32_MAX - 4U};
  in.value[0] = 10.0f;
  in.value[1] = 10.0f;
  filter_sample_t out = {0};
  Filter_Base_Update(&lp.base, &in, &out);
  assert(nearf(out.value[0], 10.0f, 1e-6f));

  /* 同时间戳不推进平滑，保持 last_output。 */
  in.value[0] = 50.0f;
  Filter_Base_Update(&lp.base, &in, &out);
  assert(nearf(out.value[0], 10.0f, 1e-6f));

  /* 跨 32 位回绕 dt = 10 ms：ch0 alpha=0.01/0.10=0.1 -> 11.0，ch1 alpha=0.5 -> 15.0。 */
  in.time_ms = 5U;
  in.value[0] = 20.0f;
  in.value[1] = 20.0f;
  Filter_Base_Update(&lp.base, &in, &out);
  assert(nearf(out.value[0], 11.0f, 1e-4f));
  assert(nearf(out.value[1], 15.0f, 1e-4f));

  Filter_Base_Reset(&lp.base);
  in.time_ms = 20U;
  in.value[0] = 99.0f;
  Filter_Base_Update(&lp.base, &in, &out);
  assert(nearf(out.value[0], 99.0f, 1e-6f));
}

static void test_chain_and_imu_cal(void) {
  filter_imu_cal_t cal;
  const filter_imu_cal_cfg_t bad_cfg = {
      .accel_offset_mps2 = {0.0f, 0.0f, 0.0f},
      .accel_scale = {1.0f, 0.05f, 1.0f},
      .gyro_bias_dps = {0.0f, 0.0f, 0.0f},
  };
  assert(Filter_ImuCal_Init(&cal, "cal", &bad_cfg) == FILTER_STATUS_INVALID_CONFIG);

  const filter_imu_cal_cfg_t cal_cfg = {
      .accel_offset_mps2 = {0.1f, -0.2f, 0.3f},
      .accel_scale = {1.0f, 2.0f, 1.0f},
      .gyro_bias_dps = {1.5f, -2.0f, 0.5f},
  };
  assert(Filter_ImuCal_Init(&cal, "cal", &cal_cfg) == FILTER_STATUS_OK);

  filter_passthrough_t pt;
  assert(Filter_Passthrough_Init(&pt, "pt") == FILTER_STATUS_OK);

  filter_chain_t chain;
  filter_base_t *dup_stages[2] = {&cal.base, &cal.base};
  assert(Filter_Chain_Init(&chain, "chain", dup_stages, 2U) == FILTER_STATUS_INVALID_CONFIG);
  filter_base_t *self_stages[1] = {&chain.base};
  assert(Filter_Chain_Init(&chain, "chain", self_stages, 1U) == FILTER_STATUS_INVALID_CONFIG);

  filter_base_t *stages[2] = {&pt.base, &cal.base};
  assert(Filter_Chain_Init(&chain, "chain", stages, 2U) == FILTER_STATUS_OK);

  filter_sample_t in = {.valid_mask = (1U << 3U) | (1U << 7U), .time_ms = 50U};
  in.value[3] = 11.5f;
  in.value[7] = 1.8f;
  filter_sample_t out = {0};
  Filter_Base_Update(&chain.base, &in, &out);
  assert(nearf(out.value[3], 10.0f, 1e-4f));
  assert(nearf(out.value[7], 4.0f, 1e-4f));
  assert(cal.corrected_frames == 1U);

  filter_imu_cal_cfg_t next_cfg = cal_cfg;
  next_cfg.gyro_bias_dps[0] = 2.5f;
  assert(Filter_ImuCal_SetConfig(&cal, &next_cfg) == FILTER_STATUS_OK);
  filter_imu_cal_cfg_t read_cfg = {0};
  Filter_ImuCal_GetConfig(&cal, &read_cfg);
  assert(nearf(read_cfg.gyro_bias_dps[0], 2.5f, 1e-6f));
}

int main(void) {
  test_passthrough_and_sanitize();
  test_lowpass_and_wraparound();
  test_chain_and_imu_cal();
  (void)puts("test_filters passed");
  return 0;
}
