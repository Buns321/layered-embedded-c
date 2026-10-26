/**
 * @file app_uart_link_contract_test.c
 * @brief F103 速通任务一 USART1 链路命令分帧契约测试。
 * @ingroup tests
 */

#include "app_uart_link.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static app_uart_command_t s_last_command;
static uint32_t s_handler_calls;
static bool s_accept_command = true;

static bool app_uart_link_testhandler(
  const app_uart_command_t *command,
  void *context
) {
  assert(command);
  assert(context == &s_handler_calls);

  s_handler_calls++;
  s_last_command = *command;
  return s_accept_command;
}

static void app_uart_link_testfeed(
  app_uart_link_t *link_h,
  const char *text
) {
  assert(link_h && text);

  for (const char *cursor = text; *cursor != '\0'; cursor++) {
    uint16_t next_head = (uint16_t)(
      (link_h->rx_head + 1u) % APP_UART_RX_RING_CAPACITY
    );
    assert(next_head != link_h->rx_tail);
    link_h->rx_ring[link_h->rx_head] = (uint8_t)*cursor;
    link_h->rx_head = next_head;
  }
}

static void app_uart_link_testreset(app_uart_link_t *link_h) {
  assert(link_h);

  memset(link_h, 0, sizeof(*link_h));
  link_h->initialized = true;
  link_h->command_handler = app_uart_link_testhandler;
  link_h->command_context = &s_handler_calls;
  link_h->last_command_status = APP_UART_LINK_COMMAND_IDLE;
  s_last_command = (app_uart_command_t){0};
  s_handler_calls = 0u;
  s_accept_command = true;
}

static void test_command_framing(void) {
  app_uart_link_t link;
  app_uart_link_testreset(&link);

  app_uart_link_testfeed(&link, "SET R_ACCEL 0.15\r\n");
  App_UART_LinkProcessCommands(&link, 10u);
  assert(s_handler_calls == 1u);
  assert(s_last_command.kind == APP_UART_COMMAND_SET);
  assert(strcmp(s_last_command.name, "R_ACCEL") == 0);
  assert(s_last_command.value == 0.15f);
  assert(link.accepted_commands == 1u);
  assert(link.last_command_status == APP_UART_LINK_COMMAND_ACCEPTED);

  app_uart_link_testfeed(&link, "RESET EKF\n");
  App_UART_LinkProcessCommands(&link, 20u);
  assert(s_handler_calls == 2u);
  assert(s_last_command.kind == APP_UART_COMMAND_RESET_EKF);
  assert(link.accepted_commands == 2u);
}

static void test_rejected_and_invalid_commands(void) {
  app_uart_link_t link;
  app_uart_link_testreset(&link);
  s_accept_command = false;

  app_uart_link_testfeed(&link, "SET Q_ANGLE 0.0001\n");
  App_UART_LinkProcessCommands(&link, 10u);
  assert(s_handler_calls == 1u);
  assert(link.rejected_commands == 1u);
  assert(link.last_command_status == APP_UART_LINK_COMMAND_REJECTED);

  app_uart_link_testfeed(&link, "NOT A COMMAND\n");
  App_UART_LinkProcessCommands(&link, 20u);
  assert(s_handler_calls == 1u);
  assert(link.rejected_commands == 2u);
  assert(
    link.last_command_status == APP_UART_LINK_COMMAND_FORMAT_ERROR
  );
}

static void test_overflow_and_ring_wrap(void) {
  app_uart_link_t link;
  app_uart_link_testreset(&link);
  link.rx_head = 200u;
  link.rx_tail = 200u;

  static const char crossing_text[] = "RESET EKF\n";
  app_uart_link_testfeed(&link, crossing_text);
  App_UART_LinkProcessCommands(&link, 10u);
  assert(s_handler_calls == 1u);
  assert(s_last_command.kind == APP_UART_COMMAND_RESET_EKF);

  app_uart_link_testreset(&link);
  char overlong[APP_UART_COMMAND_CAPACITY + 1u];
  memset(overlong, 'A', APP_UART_COMMAND_CAPACITY);
  overlong[APP_UART_COMMAND_CAPACITY] = '\0';

  app_uart_link_testfeed(&link, overlong);
  App_UART_LinkProcessCommands(&link, 20u);
  assert(link.command_length == 0u);
  assert(link.rejected_commands == 1u);
  assert(link.last_command_status == APP_UART_LINK_COMMAND_OVERFLOW);
}

void Dev_UART_Base_Init(
  uart_base_t *uart_h,
  const char *name,
  const uart_ops_t *ops
) {
  (void)uart_h;
  (void)name;
  (void)ops;
}

dev_uart_status_t Dev_UART_Base_Receive(
  uart_base_t *uart_h,
  uint8_t *buf,
  uint16_t size,
  uint32_t timeout
) {
  (void)uart_h;
  (void)buf;
  (void)size;
  (void)timeout;
  return DEV_UART_OK;
}

dev_uart_status_t Dev_UART_Base_Transmit(
  uart_base_t *uart_h,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)uart_h;
  (void)data;
  (void)size;
  (void)timeout;
  return DEV_UART_OK;
}

dev_uart_status_t Dev_UART_ReceiveIT(
  uart_base_t *uart_h,
  uint8_t *pdata,
  uint16_t size
) {
  (void)uart_h;
  (void)pdata;
  (void)size;
  return DEV_UART_OK;
}

dev_uart_status_t Dev_UART_TransmitIT(
  uart_base_t *uart_h,
  const uint8_t *pdata,
  uint16_t size
) {
  (void)uart_h;
  (void)pdata;
  (void)size;
  return DEV_UART_OK;
}

dev_uart_status_t Dev_UART_ReceiveDMA(
  uart_base_t *uart_h,
  uint8_t *pdata,
  uint16_t size
) {
  (void)uart_h;
  (void)pdata;
  (void)size;
  return DEV_UART_OK;
}

dev_uart_status_t Dev_UART_TransmitDMA(
  uart_base_t *uart_h,
  const uint8_t *pdata,
  uint16_t size
) {
  (void)uart_h;
  (void)pdata;
  (void)size;
  return DEV_UART_OK;
}

void Dev_UART_RegisterCallback(
  uart_base_t *uart_h,
  const dev_uart_callbacks_t *cb
) {
  (void)uart_h;
  (void)cb;
}

void Dev_UART_Base_OnTxComplete(uart_base_t *uart_h) {
  (void)uart_h;
}

platform_status_t Dev_UART_Port_Init(
  uart_port_t *uart_h,
  const char *name,
  platform_uart_instance_t instance
) {
  (void)uart_h;
  (void)name;
  (void)instance;
  return PLATFORM_STATUS_OK;
}

void Dev_JustFloat_UART_Init(
  justfloat_uart_t *telemetry_h,
  const char *name,
  uart_base_t *uart_h,
  uint8_t channel_count
) {
  (void)telemetry_h;
  (void)name;
  (void)uart_h;
  (void)channel_count;
}

dev_telemetry_status_t Dev_Telemetry_Base_Send(
  telemetry_base_t *telemetry_h,
  const telemetry_sample_t *sample
) {
  (void)telemetry_h;
  (void)sample;
  return DEV_TELEMETRY_OK;
}

void Dev_Telemetry_Base_GetStatus(
  telemetry_base_t *telemetry_h,
  telemetry_status_t *status
) {
  (void)telemetry_h;
  assert(status);
  *status = (telemetry_status_t){0};
}

int main(void) {
  test_command_framing();
  test_rejected_and_invalid_commands();
  test_overflow_and_ring_wrap();
  return 0;
}
