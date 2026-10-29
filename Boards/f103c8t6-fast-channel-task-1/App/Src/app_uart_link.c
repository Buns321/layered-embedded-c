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

/**
 * @brief 把 UART base 句柄还原为链路具体对象。
 */
static app_uart_link_t *app_uart_linkfromview(uart_base_t *uart_h) {
  assert(uart_h);
  return (app_uart_link_t *)uart_h;
}

/**
 * @brief 转发阻塞接收请求到 Platform UART。
 */
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

/**
 * @brief 转发阻塞发送请求到 Platform UART。
 */
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

/**
 * @brief 转发中断接收请求并注册上下文。
 */
static dev_uart_status_t app_uart_linkviewreceiveit(
  uart_base_t *uart_h,
  uint8_t *pdata,
  uint16_t size
) {
  app_uart_link_t *link_h = app_uart_linkfromview(uart_h);
  return Dev_UART_ReceiveIT(&link_h->port.uart_base, pdata, size);
}

/**
 * @brief 转发中断发送请求并注册上下文。
 */
static dev_uart_status_t app_uart_linkviewtransmitit(
  uart_base_t *uart_h,
  const uint8_t *pdata,
  uint16_t size
) {
  app_uart_link_t *link_h = app_uart_linkfromview(uart_h);
  return Dev_UART_TransmitIT(&link_h->port.uart_base, pdata, size);
}

/**
 * @brief 转发 DMA 接收请求并注册上下文。
 */
static dev_uart_status_t app_uart_linkviewreceivedma(
  uart_base_t *uart_h,
  uint8_t *pdata,
  uint16_t size
) {
  app_uart_link_t *link_h = app_uart_linkfromview(uart_h);
  return Dev_UART_ReceiveDMA(&link_h->port.uart_base, pdata, size);
}

/**
 * @brief 转发 DMA 发送请求并注册上下文。
 */
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

/**
 * @brief 重新挂接 UART 中断接收。
 */
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

/**
 * @brief 处理发送完成回调并继续链路状态机。
 */
static void app_uart_linkontxcomplete(
  uart_base_t *uart_h,
  void *context
) {
  (void)uart_h;
  assert(context);
  app_uart_link_t *link_h = (app_uart_link_t *)context;
  Dev_UART_Base_OnTxComplete(&link_h->telemetry_view);
}

/**
 * @brief 处理接收完成回调并推进命令组帧。
 */
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

/**
 * @brief 处理 UART 错误并重置链路接收状态。
 */
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

/**
 * @brief 执行已组帧的文本命令。
 */
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

/**
 * @brief 初始化 UART1 链路和 JustFloat。
 * @param link_h 对象。
 * @param name 名称。
 * @param command_handler 命令处理回调，不能为空。
 * @param command_context 回调上下文。
 * @retval PLATFORM_STATUS_OK 初始化和接收挂载成功。
 * @retval PLATFORM_STATUS_BUSY 物理 UART 回调已被占用。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 参数或 UART 实例非法。
 */
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

/**
 * @brief 启动 USART1 单字节中断接收。
 * @retval PLATFORM_STATUS_OK 接收已启动。
 * @retval PLATFORM_STATUS_BUSY UART 正忙，稍后由命令处理阶段重试。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 对象未初始化。
 * @retval PLATFORM_STATUS_ERROR UART 启动失败。
 */
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

/**
 * @brief 解析已接收参数命令。
 * @param link_h 链路对象。
 * @param now_ms 当前毫秒时间戳；当前保留，后续用于错误恢复节流。
 */
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

/** @brief 非阻塞发送一帧 32 通道 JustFloat。 */
dev_telemetry_status_t App_UART_LinkSendTelemetry(
  app_uart_link_t *link_h,
  const telemetry_sample_t *sample
) {
  assert(link_h && sample);
  return Dev_Telemetry_Base_Send(&link_h->telemetry.base, sample);
}

/**
 * @brief 复制链路状态。
 * @param link_h 链路对象。
 * @param status 输出状态。
 */
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
