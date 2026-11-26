/**
 * @file test_sys_topic.c
 * @brief sys_topic 静态类型安全 Pub-Sub 通道宿主机单元测试。
 * @ingroup tests
 */

#include "bsp_mcu.h"
#include "sys_topic.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct test_imu_payload_s {
  float yaw_rad;
  float gz_radps;
  uint8_t status;
} test_imu_payload_t;

typedef struct test_cmd_payload_s {
  float vx_mps;
  float vy_mps;
  float wz_radps;
  bool estop;
} test_cmd_payload_t;

SYS_TOPIC_DECLARE(test_imu, test_imu_payload_t);
SYS_TOPIC_DECLARE(test_cmd, test_cmd_payload_t);
SYS_TOPIC_DECLARE(test_scalar, float);

SYS_TOPIC_DEFINE(test_imu);
SYS_TOPIC_DEFINE(test_cmd);
SYS_TOPIC_DEFINE(test_scalar);

_Static_assert(_Generic((&Topic_test_imu_Publish),
                       void (*)(const test_imu_payload_t *, uint32_t): 1,
                       default: 0) == 1,
               "Topic_test_imu_Publish signature mismatch");
_Static_assert(_Generic((&Topic_test_imu_Read),
                       bool (*)(test_imu_payload_t *, sys_topic_meta_t *): 1,
                       default: 0) == 1,
               "Topic_test_imu_Read signature mismatch");
_Static_assert(_Generic((&g_topic_test_cmd.payload), test_cmd_payload_t *: 1, default: 0) == 1,
               "Topic_test_cmd payload type mismatch");
_Static_assert(_Generic((&g_topic_test_scalar.payload), float *: 1, default: 0) == 1,
               "Topic_test_scalar payload type mismatch");

static bool near_f32(float left, float right) { return fabsf(left - right) <= 1e-6f; }

static void test_initial_invalid_and_null_guards(void) {
  BSP_Stub_ResetAll();
  Topic_test_imu_Reset();
  Topic_test_cmd_Reset();
  Topic_test_scalar_Reset();

  test_imu_payload_t imu_out = {1.0f, 2.0f, 3U};
  sys_topic_meta_t meta = {99U, 88U, true};

  assert(!Topic_test_imu_Read(&imu_out, &meta));
  assert(!meta.valid);
  assert(meta.sequence == 0U);
  assert(meta.time_ms == 0U);
  assert(near_f32(imu_out.yaw_rad, 0.0f));
  assert(imu_out.status == 0U);

  assert(!Topic_test_imu_Read(NULL, &meta));
  assert(!meta.valid);
  assert(!Topic_test_imu_Read(&imu_out, NULL));
  assert(!Topic_test_cmd_Read(NULL, NULL));
  assert(!Topic_test_scalar_Read(NULL, NULL));

  Topic_test_imu_Publish(NULL, 123U);
  assert(!Topic_test_imu_Read(&imu_out, &meta));
  assert(meta.sequence == 0U);
}

static void test_publish_read_sequence_and_isolation(void) {
  BSP_Stub_ResetAll();
  Topic_test_imu_Reset();
  Topic_test_cmd_Reset();
  Topic_test_scalar_Reset();

  typedef void (*imu_pub_fn_t)(const test_imu_payload_t *p, uint32_t now_ms);
  typedef bool (*imu_read_fn_t)(test_imu_payload_t *out, sys_topic_meta_t *meta);
  imu_pub_fn_t imu_pub = Topic_test_imu_Publish;
  imu_read_fn_t imu_read = Topic_test_imu_Read;

  const test_imu_payload_t imu_first = {.yaw_rad = 0.5f, .gz_radps = -0.25f, .status = 2U};
  imu_pub(&imu_first, 100U);

  test_imu_payload_t imu_out = {.status = 0U};
  sys_topic_meta_t imu_meta = {.valid = false};
  assert(imu_read(&imu_out, &imu_meta));
  assert(imu_meta.valid);
  assert(imu_meta.sequence == 1U);
  assert(imu_meta.time_ms == 100U);
  assert(near_f32(imu_out.yaw_rad, 0.5f));
  assert(near_f32(imu_out.gz_radps, -0.25f));
  assert(imu_out.status == 2U);

  /* 未发布的其他 Topic 依然保持无效态，相互完全隔离。 */
  test_cmd_payload_t cmd_out;
  sys_topic_meta_t cmd_meta;
  assert(!Topic_test_cmd_Read(&cmd_out, &cmd_meta));
  assert(!cmd_meta.valid);
  assert(cmd_meta.sequence == 0U);

  const test_imu_payload_t imu_second = {.yaw_rad = -1.25f, .gz_radps = 0.75f, .status = 7U};
  Topic_test_imu_Publish(&imu_second, 250U);
  assert(Topic_test_imu_Read(&imu_out, &imu_meta));
  assert(imu_meta.valid);
  assert(imu_meta.sequence == 2U);
  assert(imu_meta.time_ms == 250U);
  assert(near_f32(imu_out.yaw_rad, -1.25f));
  assert(near_f32(imu_out.gz_radps, 0.75f));
  assert(imu_out.status == 7U);

  const test_cmd_payload_t cmd_in = {
      .vx_mps = 0.2f, .vy_mps = -0.1f, .wz_radps = 1.1f, .estop = true};
  Topic_test_cmd_Publish(&cmd_in, 300U);
  assert(Topic_test_cmd_Read(&cmd_out, &cmd_meta));
  assert(cmd_meta.valid && cmd_meta.sequence == 1U && cmd_meta.time_ms == 300U);
  assert(near_f32(cmd_out.vx_mps, 0.2f));
  assert(near_f32(cmd_out.vy_mps, -0.1f));
  assert(near_f32(cmd_out.wz_radps, 1.1f));
  assert(cmd_out.estop);

  const float scalar_in = 3.5f;
  float scalar_out = 0.0f;
  sys_topic_meta_t scalar_meta;
  Topic_test_scalar_Publish(&scalar_in, 400U);
  assert(Topic_test_scalar_Read(&scalar_out, &scalar_meta));
  assert(scalar_meta.sequence == 1U && scalar_meta.time_ms == 400U);
  assert(near_f32(scalar_out, 3.5f));

  /* Reset 仅复位目标通道，恢复无效初态。 */
  Topic_test_imu_Reset();
  assert(!Topic_test_imu_Read(&imu_out, &imu_meta));
  assert(!imu_meta.valid && imu_meta.sequence == 0U && imu_meta.time_ms == 0U);
  assert(Topic_test_cmd_Read(&cmd_out, &cmd_meta));
  assert(cmd_meta.sequence == 1U);
}

static void test_critical_section_balance(void) {
  BSP_Stub_ResetAll();
  Topic_test_imu_Reset();

  const test_imu_payload_t payload = {.yaw_rad = 1.0f, .gz_radps = 0.0f, .status = 1U};
  test_imu_payload_t out;
  sys_topic_meta_t meta;

  __set_PRIMASK(0U);
  Topic_test_imu_Publish(&payload, 10U);
  assert(__get_PRIMASK() == 0U);
  assert(Topic_test_imu_Read(&out, &meta));
  assert(__get_PRIMASK() == 0U);

  __set_PRIMASK(1U);
  Topic_test_imu_Publish(&payload, 20U);
  assert(__get_PRIMASK() == 1U);
  assert(Topic_test_imu_Read(&out, &meta));
  assert(__get_PRIMASK() == 1U);
  assert(meta.sequence == 2U);
  __set_PRIMASK(0U);
}

int main(void) {
  test_initial_invalid_and_null_guards();
  test_publish_read_sequence_and_isolation();
  test_critical_section_balance();
  (void)puts("test_sys_topic passed");
  return 0;
}
