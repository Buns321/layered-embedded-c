/**
 * @file test_sys_daemon.c
 * @brief sys_daemon 静态离线看门狗服务宿主机单元测试。
 * @ingroup tests
 */

#include "bsp_mcu.h"
#include "sys_daemon.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

typedef struct test_daemon_trace_s {
  sys_daemon_t *node;
  uint32_t offline_calls;
  uint32_t recover_calls;
  uint32_t last_offline_ms;
  uint32_t last_recover_ms;
  sys_daemon_state_t state_during_offline;
  sys_daemon_state_t state_during_recover;
  uint32_t primask_during_offline;
  uint32_t primask_during_recover;
} test_daemon_trace_t;

static void test_on_offline(void *ctx, uint32_t now_ms) {
  test_daemon_trace_t *trace = (test_daemon_trace_t *)ctx;
  trace->offline_calls++;
  trace->last_offline_ms = now_ms;
  trace->state_during_offline = Sys_Daemon_GetState(trace->node);
  trace->primask_during_offline = __get_PRIMASK();
}

static void test_on_recover(void *ctx, uint32_t now_ms) {
  test_daemon_trace_t *trace = (test_daemon_trace_t *)ctx;
  trace->recover_calls++;
  trace->last_recover_ms = now_ms;
  trace->state_during_recover = Sys_Daemon_GetState(trace->node);
  trace->primask_during_recover = __get_PRIMASK();
}

static void test_invalid_and_unregistered(void) {
  BSP_Stub_ResetAll();
  Sys_Daemon_ResetAll();

  sys_daemon_t node = {.registered = false};
  sys_daemon_cfg_t cfg = {
      .name = "bad",
      .timeout_ms = 100U,
      .init_grace_ms = 0U,
      .on_offline = NULL,
      .on_recover = NULL,
      .ctx = NULL,
  };

  assert(!Sys_Daemon_Register(NULL, &cfg, 0U));
  assert(!Sys_Daemon_Register(&node, NULL, 0U));
  cfg.timeout_ms = 0U;
  assert(!Sys_Daemon_Register(&node, &cfg, 0U));
  cfg.timeout_ms = 0x80000000U;
  assert(!Sys_Daemon_Register(&node, &cfg, 0U));
  cfg.timeout_ms = 100U;
  cfg.init_grace_ms = 0x80000000U;
  assert(!Sys_Daemon_Register(&node, &cfg, 0U));

  assert(Sys_Daemon_GetState(NULL) == SYS_DAEMON_OFFLINE);
  assert(!Sys_Daemon_IsOnline(NULL));
  assert(Sys_Daemon_GetState(&node) == SYS_DAEMON_OFFLINE);
  assert(!Sys_Daemon_IsOnline(&node));

  Sys_Daemon_Feed(NULL, 10U);
  Sys_Daemon_Feed(&node, 10U);
  Sys_Daemon_FeedFromISR(NULL, 10U);
  Sys_Daemon_FeedFromISR(&node, 10U);
  assert(node.feed_seq == 0U);

  cfg.init_grace_ms = 0U;
  assert(Sys_Daemon_Register(&node, &cfg, 1000U));
  assert(!Sys_Daemon_Register(&node, &cfg, 1000U));
}

static void test_init_grace_and_lifecycle(void) {
  BSP_Stub_ResetAll();
  Sys_Daemon_ResetAll();

  sys_daemon_t grace_node;
  test_daemon_trace_t grace_trace = {.node = &grace_node};

  const sys_daemon_cfg_t grace_cfg = {
      .name = "imu",
      .timeout_ms = 100U,
      .init_grace_ms = 300U,
      .on_offline = test_on_offline,
      .on_recover = test_on_recover,
      .ctx = &grace_trace,
  };
  assert(Sys_Daemon_Register(&grace_node, &grace_cfg, 1000U));
  assert(Sys_Daemon_GetState(&grace_node) == SYS_DAEMON_INIT_GRACE);
  assert(!Sys_Daemon_IsOnline(&grace_node));

  Sys_Daemon_Poll(1300U);
  assert(Sys_Daemon_GetState(&grace_node) == SYS_DAEMON_INIT_GRACE);
  assert(grace_trace.offline_calls == 0U);

  Sys_Daemon_Poll(1301U);
  assert(Sys_Daemon_GetState(&grace_node) == SYS_DAEMON_OFFLINE);
  assert(grace_node.offline_count == 1U);
  assert(grace_trace.offline_calls == 1U);
  assert(grace_trace.last_offline_ms == 1301U);
  assert(grace_trace.state_during_offline == SYS_DAEMON_OFFLINE);
  assert(grace_trace.primask_during_offline == 0U);
  assert(grace_trace.recover_calls == 0U);

  /* 持续离线期间不得重复触发 on_offline 边沿回调。 */
  Sys_Daemon_Poll(1500U);
  assert(grace_trace.offline_calls == 1U);
  assert(grace_node.offline_count == 1U);

  /* init_grace_ms == 0 时默认等于 timeout_ms。 */
  sys_daemon_t rc_node;
  test_daemon_trace_t rc_trace = {.node = &rc_node};

  const sys_daemon_cfg_t rc_cfg = {
      .name = "rc",
      .timeout_ms = 150U,
      .init_grace_ms = 0U,
      .on_offline = test_on_offline,
      .on_recover = test_on_recover,
      .ctx = &rc_trace,
  };
  assert(Sys_Daemon_Register(&rc_node, &rc_cfg, 2000U));
  assert(rc_node.cfg.init_grace_ms == 150U);

  /* 首次喂狗后在 Poll 前不应直接变态或触发回调，Poll 后进入 ONLINE 且不触发 on_recover。 */
  Sys_Daemon_Feed(&rc_node, 2050U);
  assert(Sys_Daemon_GetState(&rc_node) == SYS_DAEMON_INIT_GRACE);
  assert(rc_trace.recover_calls == 0U);

  Sys_Daemon_Poll(2100U);
  assert(Sys_Daemon_GetState(&rc_node) == SYS_DAEMON_ONLINE);
  assert(Sys_Daemon_IsOnline(&rc_node));
  assert(rc_trace.recover_calls == 0U);

  Sys_Daemon_Poll(2200U);
  assert(Sys_Daemon_IsOnline(&rc_node));

  Sys_Daemon_Poll(2201U);
  assert(Sys_Daemon_GetState(&rc_node) == SYS_DAEMON_OFFLINE);
  assert(rc_node.offline_count == 1U);
  assert(rc_trace.offline_calls == 1U);
  assert(rc_trace.last_offline_ms == 2201U);

  /* OFFLINE -> FeedFromISR -> Poll 恢复 ONLINE 并触发一次 on_recover。 */
  Sys_Daemon_FeedFromISR(&rc_node, 2250U);
  assert(Sys_Daemon_GetState(&rc_node) == SYS_DAEMON_OFFLINE);
  assert(rc_trace.recover_calls == 0U);

  Sys_Daemon_Poll(2300U);
  assert(Sys_Daemon_GetState(&rc_node) == SYS_DAEMON_ONLINE);
  assert(rc_trace.recover_calls == 1U);
  assert(rc_trace.last_recover_ms == 2300U);
  assert(rc_trace.state_during_recover == SYS_DAEMON_ONLINE);
  assert(rc_trace.primask_during_recover == 0U);

  Sys_Daemon_Poll(2350U);
  assert(rc_trace.recover_calls == 1U);

  Sys_Daemon_Poll(2401U);
  assert(Sys_Daemon_GetState(&rc_node) == SYS_DAEMON_OFFLINE);
  assert(rc_node.offline_count == 2U);
  assert(rc_trace.offline_calls == 2U);
}

static void test_stale_feed_and_wraparound(void) {
  BSP_Stub_ResetAll();
  Sys_Daemon_ResetAll();

  sys_daemon_t stale_node;
  test_daemon_trace_t stale_trace = {.node = &stale_node};

  const sys_daemon_cfg_t cfg = {
      .name = "wrap",
      .timeout_ms = 100U,
      .init_grace_ms = 500U,
      .on_offline = test_on_offline,
      .on_recover = test_on_recover,
      .ctx = &stale_trace,
  };

  /* 宽限期内喂过狗但直到喂狗时间本身已过期才 Poll，应进入 OFFLINE。 */
  assert(Sys_Daemon_Register(&stale_node, &cfg, 1000U));
  Sys_Daemon_Feed(&stale_node, 1020U);
  Sys_Daemon_Poll(1200U);
  assert(Sys_Daemon_GetState(&stale_node) == SYS_DAEMON_OFFLINE);
  assert(stale_trace.offline_calls == 1U);
  assert(stale_trace.recover_calls == 0U);

  /* 离线状态下喂入已经过期的旧时间戳，不应误恢复为 ONLINE。 */
  Sys_Daemon_Feed(&stale_node, 1050U);
  Sys_Daemon_Poll(1250U);
  assert(Sys_Daemon_GetState(&stale_node) == SYS_DAEMON_OFFLINE);
  assert(stale_trace.recover_calls == 0U);

  /* 32 位回绕与 ISR 喂狗时间戳略微超前于 Poll 的情况。 */
  Sys_Daemon_ResetAll();
  sys_daemon_t wrap_node;
  test_daemon_trace_t wrap_trace = {.node = &wrap_node};
  const sys_daemon_cfg_t wrap_cfg = {
      .name = "wrap2",
      .timeout_ms = 100U,
      .init_grace_ms = 100U,
      .on_offline = test_on_offline,
      .on_recover = test_on_recover,
      .ctx = &wrap_trace,
  };

  assert(Sys_Daemon_Register(&wrap_node, &wrap_cfg, UINT32_MAX - 50U));
  Sys_Daemon_FeedFromISR(&wrap_node, UINT32_MAX - 20U);
  Sys_Daemon_Poll(UINT32_MAX - 10U);
  assert(Sys_Daemon_IsOnline(&wrap_node));

  /* FeedFromISR 跨回绕超前于 Poll(UINT32_MAX - 2U)，不得因无符号下溢误判超时。 */
  Sys_Daemon_FeedFromISR(&wrap_node, 5U);
  Sys_Daemon_Poll(UINT32_MAX - 2U);
  assert(Sys_Daemon_IsOnline(&wrap_node));
  assert(wrap_trace.offline_calls == 0U);

  /* 自 5U 起经过 100 ms（至 105U）仍在线；至 106U 超时离线。 */
  Sys_Daemon_Poll(105U);
  assert(Sys_Daemon_IsOnline(&wrap_node));
  Sys_Daemon_Poll(106U);
  assert(Sys_Daemon_GetState(&wrap_node) == SYS_DAEMON_OFFLINE);
  assert(wrap_trace.offline_calls == 1U);
}

static void test_slot_capacity_and_reset(void) {
  BSP_Stub_ResetAll();
  Sys_Daemon_ResetAll();

  sys_daemon_t nodes[SYS_DAEMON_MAX_NODES + 1U];
  const sys_daemon_cfg_t cfg = {
      .name = "slot",
      .timeout_ms = 50U,
      .init_grace_ms = 50U,
      .on_offline = NULL,
      .on_recover = NULL,
      .ctx = NULL,
  };

  for (uint32_t i = 0U; i < SYS_DAEMON_MAX_NODES; ++i) {
    assert(Sys_Daemon_Register(&nodes[i], &cfg, 10U));
  }
  assert(!Sys_Daemon_Register(&nodes[SYS_DAEMON_MAX_NODES], &cfg, 10U));

  Sys_Daemon_ResetAll();
  assert(Sys_Daemon_Register(&nodes[SYS_DAEMON_MAX_NODES], &cfg, 20U));
  assert(Sys_Daemon_GetState(&nodes[SYS_DAEMON_MAX_NODES]) == SYS_DAEMON_INIT_GRACE);
}

int main(void) {
  test_invalid_and_unregistered();
  test_init_grace_and_lifecycle();
  test_stale_feed_and_wraparound();
  test_slot_capacity_and_reset();
  (void)puts("test_sys_daemon passed");
  return 0;
}
