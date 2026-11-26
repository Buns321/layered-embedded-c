/**
 * @file test_kinematics_omni.c
 * @brief kinematics_omni 车体/世界坐标系解算、整轮符号反转与非有限输入防护测试。
 * @ingroup tests
 */

#include "kinematics_omni.h"
#include "sys_angle.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

static bool nearf(float a, float b, float eps) { return fabsf(a - b) <= eps; }

static void test_init_validation(void) {
  kinematics_omni_t kin;
  kinematics_omni_cfg_t bad = {
      .wheel_distance_m = 0.0f,
      .wheel_theta_rad = {0.0f, 0.0f, 0.0f, 0.0f},
      .wheel_sign = {1.0f, 1.0f, 1.0f, 1.0f},
  };
  assert(!Kinematics_Omni_Init(&kin, &bad));

  bad.wheel_distance_m = 0.12f;
  bad.wheel_sign[2] = 2.0f;
  assert(!Kinematics_Omni_Init(&kin, &bad));

  const kinematics_omni_cfg_t ok = {
      .wheel_distance_m = 0.12f,
      .wheel_theta_rad = {SYS_PI * 1.75f, SYS_PI * 1.25f, SYS_PI * 0.75f, SYS_PI * 0.25f},
      .wheel_sign = {0.0f, 1.0f, -1.0f, 1.0f},
  };
  assert(Kinematics_Omni_Init(&kin, &ok));
  assert(kin.wheel_sign[0] == 1.0f);
  assert(kin.wheel_sign[2] == -1.0f);
}

static void test_body_world_and_sign_inversion(void) {
  kinematics_omni_t kin_pos;
  kinematics_omni_t kin_flip;
  const float theta[4] = {SYS_PI * 1.75f, SYS_PI * 1.25f, SYS_PI * 0.75f, SYS_PI * 0.25f};
  assert(Kinematics_Omni_Init(&kin_pos,
                              &(kinematics_omni_cfg_t){
                                  .wheel_distance_m = 0.15f,
                                  .wheel_theta_rad = {theta[0], theta[1], theta[2], theta[3]},
                                  .wheel_sign = {1.0f, 1.0f, 1.0f, 1.0f},
                              }));
  assert(Kinematics_Omni_Init(&kin_flip,
                              &(kinematics_omni_cfg_t){
                                  .wheel_distance_m = 0.15f,
                                  .wheel_theta_rad = {theta[0], theta[1], theta[2], theta[3]},
                                  .wheel_sign = {1.0f, -1.0f, 1.0f, 1.0f},
                              }));

  /* BODY 模式不依赖 yaw_rad（即使用 NaN 作为 yaw_rad 亦不影响正常解算）。 */
  float body_a[4] = {0};
  float body_b[4] = {0};
  Kinematics_Omni_Solve(&kin_pos, 1.0f, 0.5f, 2.0f, CHASSIS_FRAME_BODY, NAN, body_a);
  Kinematics_Omni_Solve(&kin_flip, 1.0f, 0.5f, 2.0f, CHASSIS_FRAME_BODY, 1.23f, body_b);
  assert(nearf(body_b[0], body_a[0], 1e-5f));
  assert(nearf(body_b[1], -body_a[1], 1e-5f));
  assert(nearf(body_b[2], body_a[2], 1e-5f));
  assert(nearf(body_b[3], body_a[3], 1e-5f));

  /* WORLD 模式在 yaw = +pi/2 时，世界 +vx 映射到车体 -vy。 */
  float world_out[4] = {0};
  float expected_body[4] = {0};
  Kinematics_Omni_Solve(&kin_pos, 0.8f, 0.0f, 1.0f, CHASSIS_FRAME_WORLD, SYS_PI * 0.5f, world_out);
  Kinematics_Omni_Solve(&kin_pos, 0.0f, -0.8f, 1.0f, CHASSIS_FRAME_BODY, 0.0f, expected_body);
  for (uint8_t i = 0U; i < 4U; ++i) {
    assert(nearf(world_out[i], expected_body[i], 1e-5f));
  }

  /* 非有限输入与 WORLD 下 NaN yaw 清零四轮输出。 */
  Kinematics_Omni_Solve(&kin_pos, NAN, 0.0f, 0.0f, CHASSIS_FRAME_BODY, 0.0f, world_out);
  for (uint8_t i = 0U; i < 4U; ++i) {
    assert(world_out[i] == 0.0f);
  }
  Kinematics_Omni_Solve(&kin_pos, 0.5f, 0.0f, 0.0f, CHASSIS_FRAME_WORLD, NAN, world_out);
  for (uint8_t i = 0U; i < 4U; ++i) {
    assert(world_out[i] == 0.0f);
  }
}

int main(void) {
  test_init_validation();
  test_body_world_and_sign_inversion();
  (void)puts("test_kinematics_omni passed");
  return 0;
}
