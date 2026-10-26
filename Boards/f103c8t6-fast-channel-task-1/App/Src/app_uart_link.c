//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file app_uart_link.c
 * @brief 板级 UART1 全双工链路实现。
 * @ingroup app
 */

#include "app_uart_link.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

static app_uart_link_t *app_uart_linkfromview(uart_base_t *uart_h) {
  assert(uart_h);
  return (app_uart_link_t *)uart_h;
}

static dev_uart_status_t app_uart_linkviewreceive(
  uart_base_t *uart_h,
  uint8_t *buf,
  uint16_t size,
  uint32_t timeout
) {
  app_uart_link_t *link_h = app_uart_linkfromview(uart_h);
  return Dev_UART_Base_Receive(
    &link_h->port.uart_base,
    buf,
    size,
    timeout
  );
}

static dev_uart_status_t app_uart_linkviewtransmit(
  uart_base_t *uart_h,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  app_uart_link_t *link_h = app_uart_linkfromview(uart_h);
  return Dev_UART_Base_Transmit(
    &link_h->port.uart_base,
    data,
    size,
    timeout
  );
}

static dev_uart_status_t app_uart_linkviewreceiveit(
  uart_base_t *uart_h,
  uint8_t *pdata,
  uint16_t size
) {
  app_uart_link_t *link_h = app_uart_linkfromview(uart_h);
  return Dev_UART_ReceiveIT(&link_h->port.uart_base, pdata, size);
}

static dev_uart_status_t app_uart_linkviewtransmitit(
  uart_base_t *uart_h,
  const uint8_t *pdata,
  uint16_t size
) {
  app_uart_link_t *link_h = app_uart_linkfromview(uart_h);
  return Dev_UART_TransmitIT(&link_h->port.uart_base, pdata, size);
}

static dev_uart_status_t app_uart_linkviewreceivedma(
  uart_base_t *uart_h,
  uint8_t *pdata,
  uint16_t size
) {
  app_uart_link_t *link_h = app_uart_linkfromview(uart_h);
  return Dev_UART_ReceiveDMA(&link_h->port.uart_base, pdata, size);
}

static dev_uart_status_t app_uart_linkviewtransmitdma(
  uart_base_t *uart_h,
  const uint8_t *pdata,
  uint16_t size
) {
  app_uart_link_t *link_h = app_uart_linkfromview(uart_h);
  return Dev_UART_TransmitDMA(&link_h->port.uart_base, pdata, size);
}

static const uart_ops_t s_app_uart_link_view_ops = {
  .receive = app_uart_linkviewreceive,
  .transmit = app_uart_linkviewtransmit,
  .receive_it = app_uart_linkviewreceiveit,
  .transmit_it = app_uart_linkviewtransmitit,
  .receive_dma = app_uart_linkviewreceivedma,
  .transmit_dma = app_uart_linkviewtransmitdma,
};

static void app_uart_linkrearmreceive(app_uart_link_t *link_h) {
  assert(link_h);

  if (
    Dev_UART_ReceiveIT(
      &link_h->port.uart_base,
      &link_h->rx_byte,
      1u
    ) == DEV_UART_OK
  ) {
    link_h->rx_rearm_pending = false;
  } else {
    link_h->rx_rearm_pending = true;
  }
}

static void app_uart_linkontxcomplete(
  uart_base_t *uart_h,
  void *context
) {
  (void)uart_h;
  assert(context);
  app_uart_link_t *link_h = (app_uart_link_t *)context;
  Dev_UART_Base_OnTxComplete(&link_h->telemetry_view);
}

static void app_uart_linkonrxcomplete(
  uart_base_t *uart_h,
  void *context
) {
  (void)uart_h;
  assert(context);

  app_uart_link_t *link_h = (app_uart_link_t *)context;
  uint16_t next_head = (uint16_t)(
    (link_h->rx_head + 1u) % APP_UART_RX_RING_CAPACITY
  );

  if (next_head == link_h->rx_tail) {
    link_h->rx_overflow_count++;
  } else {
    link_h->rx_ring[link_h->rx_head] = link_h->rx_byte;
    link_h->rx_head = next_head;
  }

  app_uart_linkrearmreceive(link_h);
}

static void app_uart_linkonerror(
  uart_base_t *uart_h,
  void *context
) {
  (void)uart_h;
  assert(context);
  app_uart_link_t *link_h = (app_uart_link_t *)context;
  link_h->uart_error_count++;
  link_h->rx_rearm_pending = true;
}

static void app_uart_linkexecuteline(
  app_uart_link_t *link_h,
  char *line
) {
  assert(link_h && line);

  app_uart_command_t command;
  if (!App_Command_Parse(line, &command)) {
    link_h->last_command_status = APP_UART_LINK_COMMAND_FORMAT_ERROR;
    link_h->rejected_commands++;
    return;
  }

  bool accepted =
    link_h->command_handler != NULL &&
    link_h->command_handler(&command, link_h->command_context);

  if (accepted) {
    link_h->last_command_status = APP_UART_LINK_COMMAND_ACCEPTED;
    link_h->accepted_commands++;
  } else {
    link_h->last_command_status = APP_UART_LINK_COMMAND_REJECTED;
    link_h->rejected_commands++;
  }
}

platform_status_t App_UART_LinkInit(
  app_uart_link_t *link_h,
  const char *name,
  app_uart_command_handler_t command_handler,
  void *command_context
) {
  if (link_h == NULL || name == NULL || command_handler == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  memset(link_h, 0, sizeof(*link_h));
  Dev_UART_Base_Init(
    &link_h->telemetry_view,
    name,
    &s_app_uart_link_view_ops
  );

  platform_status_t status = Dev_UART_Port_Init(
    &link_h->port,
    "uart1",
    APP_UART_INSTANCE
  );
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }

  dev_uart_callbacks_t callbacks = {
    .tx_complete_cb = app_uart_linkontxcomplete,
    .rx_complete_cb = app_uart_linkonrxcomplete,
    .err_cb = app_uart_linkonerror,
    .context = link_h,
  };
  Dev_UART_RegisterCallback(&link_h->port.uart_base, &callbacks);

  Dev_JustFloat_UART_Init(
    &link_h->telemetry,
    "justfloat",
    &link_h->telemetry_view,
    (uint8_t)APP_TELEMETRY_CHANNEL_COUNT
  );

  link_h->command_handler = command_handler;
  link_h->command_context = command_context;
  link_h->last_command_status = APP_UART_LINK_COMMAND_IDLE;
  link_h->initialized = true;
  return PLATFORM_STATUS_OK;
}

platform_status_t App_UART_LinkStart(app_uart_link_t *link_h) {
  if (link_h == NULL || !link_h->initialized) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  dev_uart_status_t status = Dev_UART_ReceiveIT(
    &link_h->port.uart_base,
    &link_h->rx_byte,
    1u
  );
  if (status == DEV_UART_OK) {
    link_h->rx_rearm_pending = false;
    return PLATFORM_STATUS_OK;
  }

  link_h->rx_rearm_pending = true;
  return status == DEV_UART_BUSY
    ? PLATFORM_STATUS_BUSY
    : PLATFORM_STATUS_ERROR;
}

void App_UART_LinkProcessCommands(
  app_uart_link_t *link_h,
  uint32_t now_ms
) {
  (void)now_ms;

  if (link_h == NULL || !link_h->initialized) {
    return;
  }
  if (link_h->rx_rearm_pending) {
    app_uart_linkrearmreceive(link_h);
  }

  while (link_h->rx_tail != link_h->rx_head) {
    char received = (char)link_h->rx_ring[link_h->rx_tail];
    link_h->rx_tail = (uint16_t)(
      (link_h->rx_tail + 1u) % APP_UART_RX_RING_CAPACITY
    );

    if (received == '\r') {
      continue;
    }
    if (received == '\n') {
      link_h->command_line[link_h->command_length] = '\0';
      app_uart_linkexecuteline(link_h, link_h->command_line);
      link_h->command_length = 0u;
      continue;
    }

    if (
      link_h->command_length + 1u <
      (uint16_t)APP_UART_COMMAND_CAPACITY
    ) {
      link_h->command_line[link_h->command_length++] = received;
    } else {
      link_h->command_length = 0u;
      link_h->last_command_status = APP_UART_LINK_COMMAND_OVERFLOW;
      link_h->rejected_commands++;
    }
  }
}

dev_telemetry_status_t App_UART_LinkSendTelemetry(
  app_uart_link_t *link_h,
  const telemetry_sample_t *sample
) {
  assert(link_h && sample);
  return Dev_Telemetry_Base_Send(&link_h->telemetry.base, sample);
}

void App_UART_LinkGetStatus(
  app_uart_link_t *link_h,
  app_uart_link_status_t *status
) {
  assert(link_h && status);

  *status = (app_uart_link_status_t){
    .last_command_status = link_h->last_command_status,
    .accepted_commands = link_h->accepted_commands,
    .rejected_commands = link_h->rejected_commands,
    .rx_overflow_count = link_h->rx_overflow_count,
    .uart_error_count = link_h->uart_error_count,
  };
  Dev_Telemetry_Base_GetStatus(&link_h->telemetry.base, &status->telemetry);
}
