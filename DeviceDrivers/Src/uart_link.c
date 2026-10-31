/**
 * @file uart_link.c
 * @brief UART 单字节接收、文本行组帧和虚拟 UART 转发实现。
 * @ingroup drivers_concrete
 */

#include "uart_link.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

/**
 * @brief 把虚拟 UART 视图还原为文本行链路。
 * @param uart_h 指向 uart_link_t 首成员 uart_view 的借用指针，不得为 NULL。
 * @return 与 uart_h 同地址的链路对象借用指针。
 * @pre uart_h 必须来自未销毁且已初始化的 uart_link_t。
 * @post 只建立类型视图，不修改对象或转移所有权。
 * @note 不访问硬件、不分配内存。
 */
static uart_link_t *uart_link_fromview(uart_base_t *uart_h) {
  assert(uart_h);
  return (uart_link_t *)uart_h;
}

/**
 * @brief 通过虚拟 UART 视图转发阻塞接收。
 * @param uart_h 链路虚拟 UART 视图，不得为 NULL。
 * @param buf 调用方接收缓冲区，不得为 NULL，容量至少为 size_bytes 字节。
 * @param size_bytes 接收字节数，范围 1..UINT16_MAX。
 * @param timeout_ms 超时时间，单位 ms；具体语义由物理 UART 后端定义。
 * @return 物理 UART 的接收状态。
 * @pre 调用发生在任务上下文，且物理 UART 当前未被其他操作占用。
 * @post 成功时 buf 前 size_bytes 字节有效；失败时内容可能部分更新。
 * @note 阻塞调用，不得在 ISR 使用；不复制或保留缓冲区。
 */
static dev_uart_status_t uart_link_viewreceive(
  uart_base_t *uart_h,
  uint8_t *buf,
  uint16_t size_bytes,
  uint32_t timeout_ms
) {
  uart_link_t *link_h = uart_link_fromview(uart_h);
  return Dev_UART_Base_Receive(
    &link_h->port.uart_base,
    buf,
    size_bytes,
    timeout_ms
  );
}

/**
 * @brief 通过虚拟 UART 视图转发阻塞发送。
 * @param uart_h 链路虚拟 UART 视图，不得为 NULL。
 * @param data 调用方只读发送缓冲区，不得为 NULL，容量至少为 size_bytes 字节。
 * @param size_bytes 发送字节数，范围 1..UINT16_MAX。
 * @param timeout_ms 超时时间，单位 ms；具体语义由物理 UART 后端定义。
 * @return 物理 UART 的发送状态。
 * @pre 调用发生在任务上下文，且物理 UART 当前未被其他操作占用。
 * @post 成功时全部字节已完成发送；本函数不保留 data。
 * @note 阻塞调用，不得在 ISR 使用；不复制缓冲区。
 */
static dev_uart_status_t uart_link_viewtransmit(
  uart_base_t *uart_h,
  const uint8_t *data,
  uint16_t size_bytes,
  uint32_t timeout_ms
) {
  uart_link_t *link_h = uart_link_fromview(uart_h);
  return Dev_UART_Base_Transmit(
    &link_h->port.uart_base,
    data,
    size_bytes,
    timeout_ms
  );
}

/**
 * @brief 通过虚拟 UART 视图转发中断接收启动。
 * @param uart_h 链路虚拟 UART 视图，不得为 NULL。
 * @param pdata 接收缓冲区，不得为 NULL，容量至少为 size_bytes 字节。
 * @param size_bytes 接收字节数，范围 1..UINT16_MAX。
 * @return 物理 UART 的中断接收启动状态。
 * @pre pdata 必须保持有效到完成、错误或中止事件。
 * @post 成功时物理 UART 可继续使用中断接收。
 * @note 本链路自身的文本 RX 使用独立单字节挂接，不由该虚拟入口管理。
 */
static dev_uart_status_t uart_link_viewreceiveit(
  uart_base_t *uart_h,
  uint8_t *pdata,
  uint16_t size_bytes
) {
  uart_link_t *link_h = uart_link_fromview(uart_h);
  return Dev_UART_ReceiveIT(
    &link_h->port.uart_base,
    pdata,
    size_bytes
  );
}

/**
 * @brief 通过虚拟 UART 视图转发中断发送启动。
 * @param uart_h 链路虚拟 UART 视图，不得为 NULL。
 * @param pdata 只读发送缓冲区，不得为 NULL，容量至少为 size_bytes 字节。
 * @param size_bytes 发送字节数，范围 1..UINT16_MAX。
 * @return 物理 UART 的中断发送启动状态。
 * @pre pdata 必须保持有效到完成、错误或中止事件。
 * @post 成功时发送完成事件会转发给虚拟 UART 回调。
 * @note 不复制发送缓冲区。
 */
static dev_uart_status_t uart_link_viewtransmitit(
  uart_base_t *uart_h,
  const uint8_t *pdata,
  uint16_t size_bytes
) {
  uart_link_t *link_h = uart_link_fromview(uart_h);
  return Dev_UART_TransmitIT(
    &link_h->port.uart_base,
    pdata,
    size_bytes
  );
}

/**
 * @brief 通过虚拟 UART 视图转发 DMA 接收启动。
 * @param uart_h 链路虚拟 UART 视图，不得为 NULL。
 * @param pdata DMA 接收缓冲区，不得为 NULL，容量至少为 size_bytes 字节。
 * @param size_bytes 接收字节数，范围 1..UINT16_MAX。
 * @return 物理 UART 的 DMA 接收启动状态。
 * @pre pdata 位于 DMA 可访问内存并保持有效到最终事件。
 * @post 成功时 DMA 事件会转发给虚拟 UART 回调。
 * @note 本链路自身不使用 DMA 文本接收。
 */
static dev_uart_status_t uart_link_viewreceivedma(
  uart_base_t *uart_h,
  uint8_t *pdata,
  uint16_t size_bytes
) {
  uart_link_t *link_h = uart_link_fromview(uart_h);
  return Dev_UART_ReceiveDMA(
    &link_h->port.uart_base,
    pdata,
    size_bytes
  );
}

/**
 * @brief 通过虚拟 UART 视图转发 DMA 发送启动。
 * @param uart_h 链路虚拟 UART 视图，不得为 NULL。
 * @param pdata 只读 DMA 发送缓冲区，不得为 NULL，容量至少为 size_bytes 字节。
 * @param size_bytes 发送字节数，范围 1..UINT16_MAX。
 * @return 物理 UART 的 DMA 发送启动状态。
 * @pre pdata 位于 DMA 可访问内存并保持有效到最终事件。
 * @post 成功时 DMA TX 完成事件会转发给虚拟 UART 回调。
 * @note JustFloat 通过该入口使用物理 UART 的 DMA 发送能力。
 */
static dev_uart_status_t uart_link_viewtransmitdma(
  uart_base_t *uart_h,
  const uint8_t *pdata,
  uint16_t size_bytes
) {
  uart_link_t *link_h = uart_link_fromview(uart_h);
  return Dev_UART_TransmitDMA(
    &link_h->port.uart_base,
    pdata,
    size_bytes
  );
}

static const uart_ops_t s_uart_link_view_ops = {
  .receive = uart_link_viewreceive,
  .transmit = uart_link_viewtransmit,
  .receive_it = uart_link_viewreceiveit,
  .transmit_it = uart_link_viewtransmitit,
  .receive_dma = uart_link_viewreceivedma,
  .transmit_dma = uart_link_viewtransmitdma,
};

/**
 * @brief 尝试重新挂接下一字节中断接收。
 * @param link_h 已初始化的链路对象，不得为 NULL。
 * @post 成功时 receive_pending 清零；失败时置位并递增 rearm_error_count。
 * @note 可在 RX 完成 ISR 或任务上下文调用；不阻塞、不分配内存。
 */
static void uart_link_rearmreceive(uart_link_t *link_h) {
  assert(link_h);

  dev_uart_status_t status = Dev_UART_ReceiveIT(
    &link_h->port.uart_base,
    &link_h->rx_byte,
    1u
  );
  if (status == DEV_UART_OK) {
    link_h->receive_pending = false;
    return;
  }

  link_h->receive_pending = true;
  link_h->rearm_error_count++;
}

/**
 * @brief 把 Platform UART 状态映射为链路初始化操作状态。
 * @param status 虚拟 UART 接收启动状态。
 * @return 对应的 Platform 状态。
 * @pre status 为 DEV_UART_* 已定义值。
 * @post 纯映射，不修改链路或硬件。
 * @note 未定义值按 ERROR 处理。
 */
static platform_status_t uart_link_platformstatus(dev_uart_status_t status) {
  switch (status) {
    case DEV_UART_OK:
      return PLATFORM_STATUS_OK;
    case DEV_UART_BUSY:
      return PLATFORM_STATUS_BUSY;
    case DEV_UART_TIMEOUT:
      return PLATFORM_STATUS_TIMEOUT;
    case DEV_UART_ERROR:
    default:
      return PLATFORM_STATUS_ERROR;
  }
}

/**
 * @brief 处理物理 UART TX 完成并转发到虚拟 UART。
 * @param uart_h 物理 UART 基对象；本函数不直接使用。
 * @param context 注册时传入的 uart_link_t 指针，不得为 NULL。
 * @post 虚拟 UART 的 TX 完成回调被同步分发一次。
 * @note 在 ISR 上下文执行；不阻塞、不分配内存。
 */
static void uart_link_ontxcomplete(uart_base_t *uart_h, void *context) {
  (void)uart_h;
  assert(context);
  uart_link_t *link_h = (uart_link_t *)context;
  Dev_UART_Base_OnTxComplete(&link_h->uart_view);
}

/**
 * @brief 处理单字节 RX 完成、写入环形缓冲并重挂接收。
 * @param uart_h 物理 UART 基对象；本函数不直接使用。
 * @param context 注册时传入的 uart_link_t 指针，不得为 NULL。
 * @post 队列未满时写入一个字节并推进写指针，否则递增 rx_dropped_bytes。
 * @post 随后尝试重挂下一字节接收；失败时仅保留待重挂标志供任务重试。
 * @note 在 ISR 上下文执行；环形缓冲采用 ISR 单写、任务单读模型。
 */
static void uart_link_onrxcomplete(uart_base_t *uart_h, void *context) {
  (void)uart_h;
  assert(context);
  uart_link_t *link_h = (uart_link_t *)context;

  link_h->rx_bytes++;
  (void)Sys_ByteRing_PushFromISR(&link_h->rx_ring, link_h->rx_byte);

  uart_link_rearmreceive(link_h);
}

/**
 * @brief 记录 UART 错误并请求任务重挂接收。
 * @param uart_h 物理 UART 基对象；本函数不直接使用。
 * @param context 注册时传入的 uart_link_t 指针，不得为 NULL。
 * @post uart_error_count 加一，receive_pending 置为 true。
 * @note 在 ISR 上下文执行；不在错误回调内递归启动接收。
 */
static void uart_link_onerror(uart_base_t *uart_h, void *context) {
  (void)uart_h;
  assert(context);
  uart_link_t *link_h = (uart_link_t *)context;
  link_h->uart_error_count++;
  link_h->receive_pending = true;
}

/**
 * @brief 初始化 UART 文本行链路并取得物理 UART 的唯一回调所有权。
 * @param link_h 待初始化的静态链路对象，不得为 NULL；函数会覆盖其全部字段。
 * @param config 非空配置；RX 容量须为不小于 2 的 2 的幂，文本行容量须至少为 2，回调不得为 NULL。
 * @retval PLATFORM_STATUS_OK 虚拟 UART、物理端口和回调已成功装配。
 * @retval PLATFORM_STATUS_BUSY 物理 UART 已注册其他回调所有者。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 配置字段、缓冲容量或 UART 实例非法。
 * @retval PLATFORM_STATUS_ERROR 物理 UART 端口初始化失败。
 * @pre 硬件 UART 已由 CubeMX 完成初始化；链路尚未被并发使用。
 * @post 成功时链路拥有物理 UART 回调槽，但尚未启动 RX；必须再调用 Start。
 * @post 失败时对象内容不作可用性保证，调用方不得继续使用该链路。
 * @note 不分配内存；仅可在初始化阶段单线程调用。
 */
platform_status_t Dev_UARTLink_Init(
  uart_link_t *link_h,
  const uart_link_config_t *config
) {
  if (
    link_h == NULL ||
    config == NULL ||
    config->name == NULL ||
    config->rx_buffer == NULL ||
    config->rx_capacity_bytes < 2u ||
    config->line_buffer == NULL ||
    config->line_capacity_bytes < 2u ||
    config->line_handler == NULL
  ) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  memset(link_h, 0, sizeof(*link_h));
  link_h->name = config->name;
  link_h->line_buffer = config->line_buffer;
  link_h->line_capacity_bytes = config->line_capacity_bytes;
  link_h->line_handler = config->line_handler;
  link_h->line_context = config->line_context;

  if (!Sys_ByteRing_Init(
        &link_h->rx_ring,
        config->rx_buffer,
        config->rx_capacity_bytes
      )) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  Dev_UART_Base_Init(
    &link_h->uart_view,
    config->name,
    &s_uart_link_view_ops
  );

  platform_status_t status = Dev_UART_Port_Init(
    &link_h->port,
    config->name,
    config->instance
  );
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }

  dev_uart_callbacks_t callbacks = {
    .tx_complete_cb = uart_link_ontxcomplete,
    .rx_complete_cb = uart_link_onrxcomplete,
    .err_cb = uart_link_onerror,
    .context = link_h,
  };
  Dev_UART_RegisterCallback(&link_h->port.uart_base, &callbacks);
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 启动单字节中断接收。
 * @param link_h 已成功初始化的链路对象，不得为 NULL。
 * @retval PLATFORM_STATUS_OK 接收已成功挂接。
 * @retval PLATFORM_STATUS_BUSY UART 暂时忙；链路已记录待重挂状态。
 * @retval PLATFORM_STATUS_TIMEOUT 后端以超时拒绝启动；链路已记录待重挂状态。
 * @retval PLATFORM_STATUS_ERROR 参数或硬件状态非法；链路已记录待重挂状态。
 * @pre link_h 已完成 Dev_UARTLink_Init，且当前没有并发调用 Start。
 * @post 无论返回值为何，后续 Process 都会在 receive_pending 为 true 时继续尝试重挂。
 * @note 不阻塞等待 UART 空闲，不分配内存；通常由任务初始化阶段调用一次。
 */
platform_status_t Dev_UARTLink_Start(uart_link_t *link_h) {
  assert(link_h);

  link_h->is_started = true;
  dev_uart_status_t status = Dev_UART_ReceiveIT(
    &link_h->port.uart_base,
    &link_h->rx_byte,
    1u
  );
  if (status == DEV_UART_OK) {
    link_h->receive_pending = false;
    return PLATFORM_STATUS_OK;
  }

  link_h->receive_pending = true;
  link_h->rearm_error_count++;
  return uart_link_platformstatus(status);
}

/**
 * @brief 重试接收挂接并处理全部已入队文本行。
 * @param link_h 已初始化的链路对象，不得为 NULL。
 * @param now_ms 当前单调毫秒时间戳，单位 ms；传给本次组出的全部完整行。
 * @pre 只允许单一任务消费者调用，且不得与链路重新 Init 并发。
 * @post receive_pending 为 true 时尝试重挂接收；RX 环形缓冲被消费到追平写指针。
 * @post 每个由 LF 结束的行调用一次 line_handler；该行之前的 CR 被忽略。
 * @post 超长行丢弃到下一个 LF，并递增 line_overflow_count。
 * @note 不阻塞、不分配内存；回调同步执行，不得在 ISR 调用本函数。
 */
void Dev_UARTLink_Process(uart_link_t *link_h, uint32_t now_ms) {
  assert(link_h);

  if (link_h->receive_pending) {
    uart_link_rearmreceive(link_h);
  }

  uint8_t received_byte = 0u;
  while (Sys_ByteRing_Pop(&link_h->rx_ring, &received_byte)) {
    char received = (char)received_byte;

    if (received == '\r') {
      continue;
    }

    if (received == '\n') {
      if (link_h->line_overflow) {
        link_h->line_overflow = false;
        link_h->line_length_bytes = 0u;
        continue;
      }

      link_h->line_buffer[link_h->line_length_bytes] = '\0';
      link_h->complete_lines++;
      if (link_h->line_handler(link_h->line_buffer, now_ms, link_h->line_context)) {
        link_h->accepted_lines++;
      } else {
        link_h->rejected_lines++;
      }
      link_h->line_length_bytes = 0u;
      continue;
    }

    if (link_h->line_overflow) {
      continue;
    }

    if ((uint32_t)link_h->line_length_bytes + 1u < link_h->line_capacity_bytes) {
      link_h->line_buffer[link_h->line_length_bytes++] = received;
    } else {
      link_h->line_overflow = true;
      link_h->line_overflow_count++;
    }
  }
}

/**
 * @brief 取得虚拟 UART 视图，供 JustFloat 等 TX 后端使用。
 * @param link_h 已初始化的链路对象，可为 NULL。
 * @return link_h 非空时返回其内嵌 uart_base_t 指针，否则返回 NULL。
 * @pre 返回指针仅在 link_h 生命周期内有效。
 * @post 不修改链路状态，不取得额外所有权。
 * @note 虚拟 UART 的 TX 完成事件由物理 UART 回调转发；RX 文本行由本模块消费。
 */
uart_base_t *Dev_UARTLink_GetUartView(uart_link_t *link_h) {
  return link_h == NULL ? NULL : &link_h->uart_view;
}

/**
 * @brief 复制链路状态计数。
 * @param link_h 已初始化的链路对象，不得为 NULL。
 * @param status 调用方拥有的输出快照，不得为 NULL；函数覆盖其全部字段。
 * @pre status 可写且不与 link_h 重叠。
 * @post 输出包含启动、重挂、收发和行处理计数。
 * @note 各 volatile 字段独立读取，不保证跨字段来自同一精确时刻；不分配内存。
 */
void Dev_UARTLink_GetStatus(
  const uart_link_t *link_h,
  uart_link_status_t *status
) {
  assert(link_h && status);

  *status = (uart_link_status_t){
    .is_started = link_h->is_started,
    .receive_pending = link_h->receive_pending,
    .rx_bytes = link_h->rx_bytes,
    .rx_dropped_bytes = Sys_ByteRing_GetDroppedBytes(&link_h->rx_ring),
    .line_overflow_count = link_h->line_overflow_count,
    .uart_error_count = link_h->uart_error_count,
    .rearm_error_count = link_h->rearm_error_count,
    .complete_lines = link_h->complete_lines,
    .accepted_lines = link_h->accepted_lines,
    .rejected_lines = link_h->rejected_lines,
  };
}
