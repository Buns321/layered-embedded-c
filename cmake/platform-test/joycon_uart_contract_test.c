/**
 * @file joycon_uart_contract_test.c
 * @brief JoyCon UART ASCII 解析与快照契约的主机原生测试。
 * @ingroup tests
 */

#include "joycon_uart.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

static uint8_t *s_rx_buffer;
static uint16_t s_rx_size;
static uint32_t s_receive_calls;
static uint32_t s_critical_nesting;
static dev_uart_status_t s_receive_status;

void Sys_Critical_Enter(void) {
  s_critical_nesting++;
}

void Sys_Critical_Exit(void) {
  if (s_critical_nesting > 0u) {
    s_critical_nesting--;
  }
}

static dev_uart_status_t fake_receive_it(
  uart_base_t *uart_h,
  uint8_t *data,
  uint16_t size
) {
  (void)uart_h;

  s_rx_buffer = data;
  s_rx_size = size;
  s_receive_calls++;
  return s_receive_status;
}

static const uart_ops_t s_fake_uart_ops = {
  .receive_it = fake_receive_it,
};

static int fake_uart_reset(
  uart_base_t *uart_h,
  joycon_uart_t *joycon_h
) {
  s_rx_buffer = NULL;
  s_rx_size = 0u;
  s_receive_calls = 0u;
  s_critical_nesting = 0u;
  s_receive_status = DEV_UART_OK;

  Dev_UART_Base_Init(uart_h, "test_uart", &s_fake_uart_ops);
  Dev_JoyCon_UART_Init(joycon_h, "test_joycon", uart_h);
  if (Dev_JoyCon_UART_Start(joycon_h) != DEV_UART_OK) {
    return 1;
  }
  if (s_rx_buffer != &joycon_h->rx_byte || s_rx_size != 1u) {
    return 1;
  }
  if (uart_h->callbacks.rx_complete_cb == NULL ||
      uart_h->callbacks.err_cb == NULL) {
    return 1;
  }

  return 0;
}

static void feed_rx_byte(
  uart_base_t *uart_h,
  joycon_uart_t *joycon_h,
  uint8_t byte
) {
  (void)joycon_h;

  *s_rx_buffer = byte;
  uart_h->callbacks.rx_complete_cb(
    uart_h,
    uart_h->callbacks.context
  );
}

static void feed_text(
  uart_base_t *uart_h,
  joycon_uart_t *joycon_h,
  const char *text
) {
  size_t length = strlen(text);
  for (size_t i = 0u; i < length; i++) {
    feed_rx_byte(uart_h, joycon_h, (uint8_t)text[i]);
  }
}

static int test_valid_frames_and_optional_newline(void) {
  uart_base_t uart = {0};
  joycon_uart_t joycon = {0};
  joycon_input_t input = {0};

  CHECK(fake_uart_reset(&uart, &joycon) == 0);

  feed_text(&uart, &joycon, "[j,100,-100,50,0]\r\n");
  Dev_JoyCon_UART_Process(&joycon, 100u);

  CHECK(Dev_JoyCon_Base_GetInput(&joycon.base, &input));
  CHECK(input.left_x == 1.0f);
  CHECK(input.left_y == -1.0f);
  CHECK(input.right_x == 0.5f);
  CHECK(input.right_y == 0.0f);
  CHECK(input.time_ms == 100u);
  CHECK(input.sequence == 1u);
  CHECK(s_critical_nesting == 0u);

  feed_text(&uart, &joycon, "[J,-25,12.5,-0.5,99]");
  Dev_JoyCon_UART_Process(&joycon, 200u);
  CHECK(Dev_JoyCon_Base_GetInput(&joycon.base, &input));
  CHECK(input.left_x == -0.25f);
  CHECK(input.left_y == 0.125f);
  CHECK(input.right_x == -0.005f);
  CHECK(input.right_y == 0.99f);
  CHECK(input.time_ms == 200u);
  CHECK(input.sequence == 2u);
  return 0;
}

static int test_invalid_frames_keep_last_snapshot(void) {
  uart_base_t uart = {0};
  joycon_uart_t joycon = {0};
  joycon_input_t input = {0};
  joycon_status_t status = {0};
  const char *invalid_frames[] = {
    "[x,1,2,3,4]",
    "[j,101,0,0,0]",
    "[j,-101,0,0,0]",
    "[j,1,2,3]",
    "[j,1,2,3,4,]",
    "[j,1.2.3,0,0,0]",
    "[j,-,2,3,4]",
  };

  CHECK(fake_uart_reset(&uart, &joycon) == 0);

  feed_text(&uart, &joycon, "[j,10,20,30,40]");
  Dev_JoyCon_UART_Process(&joycon, 10u);
  CHECK(Dev_JoyCon_UART_GetInput(&joycon, &input));
  CHECK(input.sequence == 1u);

  for (uint32_t i = 0u; i < (uint32_t)(sizeof(invalid_frames) / sizeof(invalid_frames[0])); i++) {
    feed_text(&uart, &joycon, invalid_frames[i]);
    Dev_JoyCon_UART_Process(&joycon, 20u + i);
  }

  CHECK(Dev_JoyCon_UART_GetInput(&joycon, &input));
  CHECK(input.sequence == 1u);
  CHECK(input.time_ms == 10u);

  Dev_JoyCon_Base_GetStatus(&joycon.base, &status);
  CHECK(status.invalid_frames == 7u);
  CHECK(status.parsed_frames == 1u);
  return 0;
}

static int test_partial_and_overlong_frames(void) {
  uart_base_t uart = {0};
  joycon_uart_t joycon = {0};
  joycon_input_t input = {0};
  joycon_status_t status = {0};

  CHECK(fake_uart_reset(&uart, &joycon) == 0);

  feed_text(&uart, &joycon, "[j,1,2");
  feed_text(&uart, &joycon, "[j,3,4,5,6]");
  Dev_JoyCon_UART_Process(&joycon, 30u);

  CHECK(Dev_JoyCon_UART_GetInput(&joycon, &input));
  CHECK(input.left_x == 0.03f);
  CHECK(input.left_y == 0.04f);
  CHECK(input.right_x == 0.05f);
  CHECK(input.right_y == 0.06f);

  feed_text(&uart, &joycon, "[j,");
  for (uint32_t i = 0u; i < JOYCON_UART_FRAME_CAPACITY; i++) {
    feed_rx_byte(&uart, &joycon, (uint8_t)'0');
  }
  feed_text(&uart, &joycon, "]");
  Dev_JoyCon_UART_Process(&joycon, 40u);

  Dev_JoyCon_UART_GetStatus(&joycon, &status);
  CHECK(status.invalid_frames >= 2u);
  CHECK(status.sequence == 1u);
  return 0;
}

static int test_uart_error_and_rearm(void) {
  uart_base_t uart = {0};
  joycon_uart_t joycon = {0};
  joycon_status_t status = {0};
  uint32_t receive_calls_before = 0u;

  CHECK(fake_uart_reset(&uart, &joycon) == 0);
  receive_calls_before = s_receive_calls;

  uart.callbacks.err_cb(&uart, uart.callbacks.context);
  CHECK(s_receive_calls == receive_calls_before + 1u);

  Dev_JoyCon_UART_GetStatus(&joycon, &status);
  CHECK(status.uart_error_count == 1u);
  return 0;
}

static int test_start_failure_is_recoverable(void) {
  uart_base_t uart = {0};
  joycon_uart_t joycon = {0};
  joycon_status_t status = {0};

  Dev_UART_Base_Init(&uart, "test_uart", &s_fake_uart_ops);
  Dev_JoyCon_UART_Init(&joycon, "test_joycon", &uart);
  s_receive_status = DEV_UART_ERROR;

  CHECK(Dev_JoyCon_UART_Start(&joycon) == DEV_UART_ERROR);
  CHECK(joycon.rx_rearm_pending);

  Dev_JoyCon_UART_GetStatus(&joycon, &status);
  CHECK(status.is_started);
  CHECK(status.rearm_error_count == 1u);

  s_receive_status = DEV_UART_OK;
  Dev_JoyCon_UART_Process(&joycon, 50u);

  CHECK(!joycon.rx_rearm_pending);
  Dev_JoyCon_UART_GetStatus(&joycon, &status);
  CHECK(status.rearm_error_count == 1u);
  return 0;
}

int main(void) {
  if (test_valid_frames_and_optional_newline() != 0) {
    return 1;
  }
  if (test_invalid_frames_keep_last_snapshot() != 0) {
    return 1;
  }
  if (test_partial_and_overlong_frames() != 0) {
    return 1;
  }
  if (test_uart_error_and_rearm() != 0) {
    return 1;
  }
  if (test_start_failure_is_recoverable() != 0) {
    return 1;
  }

  (void)puts("joycon uart contract tests passed");
  return 0;
}
