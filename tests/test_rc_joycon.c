/**
 * @file test_rc_joycon.c
 * @brief rc_joycon 协议解析、边界/溢出/非法帧与 Daemon 喂狗/掉线测试。
 * @ingroup tests
 */

#include "rc_joycon.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void feed_str(rc_joycon_t *rc, const char *s, uint32_t now_ms) {
  for (size_t i = 0U; i < strlen(s); ++i) {
    RC_JoyCon_FeedByte(rc, (uint8_t)s[i], now_ms);
  }
}

static void push_uart_str(UART_HandleTypeDef *huart, const char *s) {
  for (size_t i = 0U; i < strlen(s); ++i) {
    *huart->rx_buf = (uint8_t)s[i];
    HAL_UART_RxCpltCallback(huart);
  }
}

static void test_parser_and_errors(void) {
  BSP_Stub_ResetAll();
  BSP_UART_ResetSlots();
  Sys_Daemon_ResetAll();

  UART_HandleTypeDef huart = {0};
  rc_joycon_t rc;
  assert(RC_JoyCon_Init(&rc, &(rc_joycon_cfg_t){.huart = &huart}) == BSP_OK);

  rc_joycon_input_t in = {0};
  assert(!RC_JoyCon_GetInput(&rc, &in));

  feed_str(&rc, "[j,50,-25.5,100,-100]\r\n", 100U);
  assert(RC_JoyCon_GetInput(&rc, &in));
  assert(fabsf(in.left_x_norm - 0.5f) < 1e-5f);
  assert(fabsf(in.left_y_norm + 0.255f) < 1e-5f);
  assert(fabsf(in.right_x_norm - 1.0f) < 1e-5f);
  assert(fabsf(in.right_y_norm + 1.0f) < 1e-5f);

  /* 非法帧：错误前缀、越界、少字段、64 字节溢出与半帧嵌套 `[` 重同步。 */
  feed_str(&rc, "[x,0,0,0,0][j,101,0,0,0][j,1,2,3]", 110U);
  char long_frame[80];
  (void)memset(long_frame, '1', sizeof(long_frame));
  long_frame[0] = '[';
  long_frame[78] = ']';
  long_frame[79] = '\0';
  feed_str(&rc, long_frame, 120U);

  /* 丢弃半帧后紧跟有效帧。 */
  feed_str(&rc, "[j,10[J, 20, -20, 0, 0]", 130U);
  rc_joycon_status_t st = {0};
  RC_JoyCon_GetStatus(&rc, &st);
  assert(st.parsed_frames == 2U);
  assert(st.invalid_frames == 5U);
  assert(RC_JoyCon_GetInput(&rc, &in) && fabsf(in.left_x_norm - 0.2f) < 1e-5f);
}

static void test_uart_and_daemon_watchdog(void) {
  BSP_Stub_ResetAll();
  BSP_UART_ResetSlots();
  Sys_Daemon_ResetAll();

  UART_HandleTypeDef huart = {0};
  sys_daemon_t rc_daemon;
  assert(Sys_Daemon_Register(&rc_daemon, &(sys_daemon_cfg_t){.name = "rc", .timeout_ms = 200U},
                             1000U));

  rc_joycon_t rc;
  assert(RC_JoyCon_Init(&rc, &(rc_joycon_cfg_t){.huart = &huart, .daemon = &rc_daemon}) == BSP_OK);
  assert(RC_JoyCon_Start(&rc) == BSP_OK);

  push_uart_str(&huart, "[j,0,50,0,0]");
  RC_JoyCon_Process(&rc, 1050U);
  Sys_Daemon_Poll(1060U);
  assert(Sys_Daemon_IsOnline(&rc_daemon));

  /* 断连超时掉线，恢复帧后重回 ONLINE。 */
  Sys_Daemon_Poll(1251U);
  assert(Sys_Daemon_GetState(&rc_daemon) == SYS_DAEMON_OFFLINE);

  push_uart_str(&huart, "[j,10,0,0,0]");
  RC_JoyCon_Process(&rc, 1300U);
  Sys_Daemon_Poll(1310U);
  assert(Sys_Daemon_IsOnline(&rc_daemon));
}

int main(void) {
  test_parser_and_errors();
  test_uart_and_daemon_watchdog();
  (void)puts("test_rc_joycon passed");
  return 0;
}
