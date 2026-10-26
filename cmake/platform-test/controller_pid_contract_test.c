/**
 * @file controller_pid_contract_test.c
 * @brief 位置式 PID 控制律的主机原生契约测试。
 * @ingroup tests
 */

#include "controller_pid.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

#define CHECK(condition) do {                                                \
  if (!(condition)) {                                                        \
    (void)fprintf(                                                           \
      stderr,                                                                \
      "%s:%d: check failed: %s\n",                                           \
      __FILE__,                                                              \
      __LINE__,                                                              \
      #condition                                                             \
    );                                                                       \
    return 1;                                                                \
  }                                                                          \
} while (0)

static void init_pid(
  controller_pid_t *controller,
  float kp,
  float ki,
  float kd,
  float max_out,
  float min_out,
  float max_integral,
  float derivative_filter_tau
) {
  Ctrl_PID_Init(
    controller,
    "test",
    kp,
    ki,
    kd,
    10.0f,
    -10.0f,
    max_out,
    min_out,
    max_integral,
    derivative_filter_tau
  );
}

static int test_proportional_and_dispatch(void) {
  controller_pid_t controller = {0};
  init_pid(
    &controller,
    100.0f,
    0.0f,
    0.0f,
    1000.0f,
    -1000.0f,
    0.0f,
    0.0f
  );
  float out = 0.0f;

  CHECK(
    Ctrl_Base_Update(&controller.controller_base, 1.0f, 0.0f, 0.01f, &out) ==
    CONTROLLER_STATUS_OK
  );
  CHECK(fabsf(out - 100.0f) < 0.001f);

  CHECK(
    Ctrl_Base_Update(&controller.controller_base, 1.0f, 1.0f, 0.01f, &out) ==
    CONTROLLER_STATUS_OK
  );
  CHECK(fabsf(out) < 0.001f);
  return 0;
}

static int test_integral_and_limit(void) {
  controller_pid_t controller = {0};
  init_pid(
    &controller,
    0.0f,
    100.0f,
    0.0f,
    1000.0f,
    -1000.0f,
    25.0f,
    0.0f
  );
  float out = 0.0f;

  CHECK(
    Ctrl_Base_Update(&controller.controller_base, 1.0f, 0.0f, 0.1f, &out) ==
    CONTROLLER_STATUS_OK
  );
  CHECK(fabsf(out - 10.0f) < 0.001f);

  for (uint8_t i = 0u; i < 10u; i++) {
    (void)Ctrl_Base_Update(
      &controller.controller_base,
      1.0f,
      0.0f,
      0.1f,
      &out
    );
  }

  CHECK(fabsf(out - 25.0f) < 0.001f);
  CHECK(fabsf(controller.integral - 25.0f) < 0.001f);
  return 0;
}

static int test_output_saturation_antwindup(void) {
  controller_pid_t controller = {0};
  init_pid(
    &controller,
    0.0f,
    100.0f,
    0.0f,
    50.0f,
    -50.0f,
    1000.0f,
    0.0f
  );
  float out = 0.0f;

  for (uint8_t i = 0u; i < 20u; i++) {
    controller_status_t status = Ctrl_Base_Update(
      &controller.controller_base,
      1.0f,
      0.0f,
      0.1f,
      &out
    );

    if (i >= 5u) {
      CHECK(status == CONTROLLER_STATUS_SATURATED);
    }
  }

  CHECK(fabsf(out - 50.0f) < 0.001f);
  CHECK(controller.integral <= 50.001f);

  controller_status_t status = Ctrl_Base_Update(
    &controller.controller_base,
    0.0f,
    1.0f,
    0.1f,
    &out
  );
  CHECK(status == CONTROLLER_STATUS_OK);
  CHECK(out < 50.0f);
  return 0;
}

static int test_derivative_on_measurement(void) {
  controller_pid_t controller = {0};
  init_pid(
    &controller,
    0.0f,
    0.0f,
    100.0f,
    1000.0f,
    -1000.0f,
    0.0f,
    0.0f
  );
  float out = 0.0f;

  CHECK(
    Ctrl_Base_Update(&controller.controller_base, 0.0f, 0.0f, 0.1f, &out) ==
    CONTROLLER_STATUS_OK
  );
  CHECK(fabsf(out) < 0.001f);

  CHECK(
    Ctrl_Base_Update(&controller.controller_base, 0.0f, 0.1f, 0.1f, &out) ==
    CONTROLLER_STATUS_OK
  );
  CHECK(fabsf(out + 100.0f) < 0.01f);
  return 0;
}

static int test_derivative_filter(void) {
  controller_pid_t controller = {0};
  init_pid(
    &controller,
    0.0f,
    0.0f,
    100.0f,
    1000.0f,
    -1000.0f,
    0.0f,
    0.1f
  );
  float out = 0.0f;

  (void)Ctrl_Base_Update(
    &controller.controller_base,
    0.0f,
    0.0f,
    0.1f,
    &out
  );
  CHECK(
    Ctrl_Base_Update(&controller.controller_base, 0.0f, 0.1f, 0.1f, &out) ==
    CONTROLLER_STATUS_OK
  );
  CHECK(fabsf(out + 50.0f) < 0.01f);
  return 0;
}

static int test_target_clamp_and_invalid_input(void) {
  controller_pid_t controller = {0};
  init_pid(
    &controller,
    10.0f,
    0.0f,
    0.0f,
    1000.0f,
    -1000.0f,
    0.0f,
    0.0f
  );
  float out = 0.0f;

  CHECK(
    Ctrl_Base_Update(&controller.controller_base, 20.0f, 0.0f, 0.01f, &out) ==
    CONTROLLER_STATUS_SATURATED
  );
  CHECK(fabsf(out - 100.0f) < 0.001f);

  CHECK(
    Ctrl_Base_Update(&controller.controller_base, NAN, 0.0f, 0.01f, &out) ==
    CONTROLLER_STATUS_ERROR
  );
  CHECK(fabsf(out) < 0.001f);
  CHECK(!controller.has_last_measure);
  CHECK(controller.integral == 0.0f);
  return 0;
}

static int test_reset(void) {
  controller_pid_t controller = {0};
  init_pid(
    &controller,
    0.0f,
    100.0f,
    0.0f,
    1000.0f,
    -1000.0f,
    100.0f,
    0.0f
  );
  float out = 0.0f;

  (void)Ctrl_Base_Update(
    &controller.controller_base,
    1.0f,
    0.0f,
    0.1f,
    &out
  );
  CHECK(controller.integral > 0.0f);
  CHECK(controller.has_last_measure);

  Ctrl_Base_Reset(&controller.controller_base);
  CHECK(controller.integral == 0.0f);
  CHECK(controller.filtered_derivative == 0.0f);
  CHECK(controller.last_out == 0.0f);
  CHECK(!controller.has_last_measure);
  return 0;
}

int main(void) {
  if (test_proportional_and_dispatch() != 0) {
    return 1;
  }
  if (test_integral_and_limit() != 0) {
    return 1;
  }
  if (test_output_saturation_antwindup() != 0) {
    return 1;
  }
  if (test_derivative_on_measurement() != 0) {
    return 1;
  }
  if (test_derivative_filter() != 0) {
    return 1;
  }
  if (test_target_clamp_and_invalid_input() != 0) {
    return 1;
  }
  if (test_reset() != 0) {
    return 1;
  }

  (void)puts("controller pid contract tests passed");
  return 0;
}
