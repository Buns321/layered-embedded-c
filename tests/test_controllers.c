/**
 * @file test_controllers.c
 * @brief controller_base / pid / linear / compose 宿主机单元测试。
 * @ingroup tests
 */

#include "controller_compose.h"
#include "controller_linear.h"
#include "controller_pid.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static bool nearf(float a, float b, float eps) { return fabsf(a - b) <= eps; }

static void test_pid_behavior_and_anti_windup(void) {
  controller_pid_t pid;
  ctrl_pid_cfg_t bad = {.kp = -1.0f, .max_out = 100.0f};
  assert(!Ctrl_PID_Init(&pid, "pid", &bad));
  bad = (ctrl_pid_cfg_t){.kp = 1.0f, .max_out = 0.0f};
  assert(!Ctrl_PID_Init(&pid, "pid", &bad));

  /* 验证默认限幅：min_out=-max_out、max_integral=max_out、目标不限幅。 */
  const ctrl_pid_cfg_t cfg = {
      .kp = 10.0f,
      .ki = 50.0f,
      .kd = 2.0f,
      .max_out = 100.0f,
      .derivative_tau_s = 0.02f,
  };
  assert(Ctrl_PID_Init(&pid, "pid", &cfg));
  assert(pid.min_out == -100.0f);
  assert(pid.max_integral == 100.0f);
  assert(!pid.limit_target);

  /* 首拍目标跳变不产生微分冲击，且输出饱和时误差继续推向饱和应冻结积分。 */
  float out = 0.0f;
  assert(Ctrl_Base_Update(&pid.base, 20.0f, 0.0f, 0.01f, &out) == CONTROLLER_STATUS_SATURATED);
  assert(nearf(out, 100.0f, 1e-4f));
  float frozen_i = pid.integral;
  assert(frozen_i == 0.0f);

  /* 误差反向推动退饱和时积分允许更新。 */
  assert(Ctrl_Base_Update(&pid.base, 20.0f, 35.0f, 0.01f, &out) == CONTROLLER_STATUS_SATURATED);
  assert(nearf(out, -100.0f, 1e-4f));
  assert(pid.integral == 0.0f);

  /* 有限时间步下微分低通生效。 */
  Ctrl_Base_Reset(&pid.base);
  assert(Ctrl_Base_Update(&pid.base, 1.0f, 0.0f, 0.01f, &out) == CONTROLLER_STATUS_OK);
  assert(nearf(out, 10.5f, 1e-3f));
  assert(Ctrl_Base_Update(&pid.base, 1.0f, 0.5f, 0.01f, &out) == CONTROLLER_STATUS_OK);
  assert(pid.filtered_derivative < 0.0f);

  /* NaN 与非法 dt_s 防护清零状态与输出。 */
  assert(Ctrl_Base_Update(&pid.base, NAN, 0.0f, 0.01f, &out) == CONTROLLER_STATUS_ERROR);
  assert(out == 0.0f && !pid.has_last_measure && pid.integral == 0.0f);
  assert(Ctrl_Base_Update(&pid.base, 1.0f, 0.0f, 0.0f, &out) == CONTROLLER_STATUS_ERROR);
  assert(out == 0.0f);
}

static void test_linear_controller(void) {
  controller_linear_t lin;
  const ctrl_linear_cfg_t bad = {.k = 10.0f, .max_out = 100.0f, .max_step = 5.0f, .deadzone = 6.0f};
  assert(!Ctrl_Linear_Init(&lin, "lin", &bad));

  const ctrl_linear_cfg_t cfg = {
      .k = 100.0f,
      .b = 10.0f,
      .defriction_enabled = true,
      .max_target = 2.0f,
      .max_out = 150.0f,
      .max_step = 40.0f,
      .min_step = 15.0f,
      .deadzone = 5.0f,
  };
  assert(Ctrl_Linear_Init(&lin, "lin", &cfg));

  float out = 0.0f;
  /* 0.2*100+10=30 -> 受 max_step=40 允许，首次直接输出 30。 */
  assert(Ctrl_Base_Update(&lin.base, 0.2f, 0.0f, 0.01f, &out) == CONTROLLER_STATUS_OK);
  assert(nearf(out, 30.0f, 1e-4f));
  /* 目标变化落入 deadzone (|33-30|=3 < 5) 时保持 last_out=30。 */
  assert(Ctrl_Base_Update(&lin.base, 0.23f, 0.0f, 0.01f, &out) == CONTROLLER_STATUS_OK);
  assert(nearf(out, 30.0f, 1e-4f));
  /* 落入 [deadzone, min_step) 时按 min_step=15 跃迁并报饱和。 */
  assert(Ctrl_Base_Update(&lin.base, 0.28f, 0.0f, 0.01f, &out) == CONTROLLER_STATUS_SATURATED);
  assert(nearf(out, 45.0f, 1e-4f));
  /* 超 max_target 与 max_step 触发饱和。 */
  assert(Ctrl_Base_Update(&lin.base, 5.0f, 0.0f, 0.01f, &out) == CONTROLLER_STATUS_SATURATED);
  assert(nearf(out, 85.0f, 1e-4f));
  assert(Ctrl_Base_Update(&lin.base, NAN, 0.0f, 0.01f, &out) == CONTROLLER_STATUS_ERROR);
  assert(out == 0.0f && lin.last_out == 0.0f);
}

static void test_compose_controller(void) {
  controller_pid_t pid;
  controller_linear_t lin;
  controller_compose_t comp;
  assert(Ctrl_PID_Init(&pid, "p", &(ctrl_pid_cfg_t){.kp = 10.0f, .max_out = 50.0f}));
  assert(Ctrl_Linear_Init(&lin, "l", &(ctrl_linear_cfg_t){.k = 20.0f, .max_out = 100.0f}));

  controller_base_t *dup[2] = {&pid.base, &pid.base};
  assert(!Ctrl_Compose_Init(&comp, "c", dup, 2U));
  controller_base_t *self_ref[1] = {&comp.base};
  assert(!Ctrl_Compose_Init(&comp, "c", self_ref, 1U));

  controller_base_t *stages[2] = {&pid.base, &lin.base};
  assert(Ctrl_Compose_Init(&comp, "c", stages, 2U));

  float out = 0.0f;
  /* target=2, measure=1 => pid=10, lin=40 => sum=50。 */
  assert(Ctrl_Base_Update(&comp.base, 2.0f, 1.0f, 0.01f, &out) == CONTROLLER_STATUS_OK);
  assert(nearf(out, 50.0f, 1e-4f));
  /* target=10, measure=0 => pid 饱和到 50, lin=100 => 报饱和, sum=150。 */
  assert(Ctrl_Base_Update(&comp.base, 10.0f, 0.0f, 0.01f, &out) == CONTROLLER_STATUS_SATURATED);
  assert(nearf(out, 150.0f, 1e-4f));
  /* NaN 错误短路并清零输出。 */
  assert(Ctrl_Base_Update(&comp.base, NAN, 0.0f, 0.01f, &out) == CONTROLLER_STATUS_ERROR);
  assert(out == 0.0f);
}

int main(void) {
  test_pid_behavior_and_anti_windup();
  test_linear_controller();
  test_compose_controller();
  (void)puts("test_controllers passed");
  return 0;
}
