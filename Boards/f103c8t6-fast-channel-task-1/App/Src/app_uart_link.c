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
 * @brief 把 UART base 句柄还原为链路具体实例。
 * @param uart_h 指向链路首成员 telemetry_view 的 UART 句柄，必须非 NULL。
 * @return 与 uart_h 同地址的 app_uart_link_t 指针；不会返回 NULL。
 * @pre uart_h 必须来自 app_uart_link_t.telemetry_view，调试构建以 assert 检查非空。
 * @note 依赖 telemetry_view 为首成员的布局契约；仅做静态回指，不分配内存、不修改链路实例。
 */
static app_uart_link_t *app_uart_linkfromview(uart_base_t *uart_h) {
  assert(uart_h);
  return (app_uart_link_t *)uart_h;
}

/**
 * @brief 转发阻塞接收请求到 Platform UART。
 * @param uart_h 链路虚拟 UART 句柄，必须非 NULL。
 * @param buf 接收缓冲区，必须可写且容量至少为 size 字节。
 * @param size 请求接收的字节数，必须大于零并可由物理 UART 接受。
 * @param timeout 阻塞超时，单位 ms。
 * @retval DEV_UART_OK 接收完成。
 * @retval DEV_UART_BUSY 物理 UART 已被其他操作占用。
 * @retval DEV_UART_TIMEOUT 超时前未收到完整数据。
 * @retval DEV_UART_ERROR 参数或底层 UART 状态非法。
 * @pre 仅可在任务上下文调用；buf 在整个调用期间保持有效。
 * @note 不分配内存；本转发不接管缓冲区，也不修改链路命令状态。
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
 * @param uart_h 链路虚拟 UART 句柄，必须非 NULL。
 * @param data 待发送数据，只读且至少包含 size 字节。
 * @param size 请求发送的字节数，必须大于零并可由物理 UART 接受。
 * @param timeout 阻塞超时，单位 ms。
 * @retval DEV_UART_OK 数据已全部发送。
 * @retval DEV_UART_BUSY 物理 UART 已被其他操作占用。
 * @retval DEV_UART_TIMEOUT 超时前未完成发送。
 * @retval DEV_UART_ERROR 参数或底层 UART 状态非法。
 * @pre 仅可在任务上下文调用；data 在整个调用期间保持有效。
 * @note 不分配内存；本函数与 JustFloat 共用同一物理 UART，调用方必须遵守单一发送所有者模型。
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
 * @brief 转发中断接收请求到 Platform UART。
 * @param uart_h 链路虚拟 UART 句柄，必须非 NULL。
 * @param pdata 接收缓冲区，可写且容量至少为 size 字节。
 * @param size 中断接收字节数，必须大于零。
 * @retval DEV_UART_OK 接收请求已被底层接受。
 * @retval DEV_UART_BUSY 物理 UART 正忙。
 * @retval DEV_UART_ERROR 参数或实例状态非法。
 * @pre pdata 必须保持有效直到完成回调或错误回调触发。
 * @note 不分配内存；完成/错误回调在 UART ISR 上下文分发，ISR 与任务对该缓冲区的并发访问由调用方排除。
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
 * @brief 转发中断发送请求到 Platform UART。
 * @param uart_h 链路虚拟 UART 句柄，必须非 NULL。
 * @param pdata 待发送数据，只读且至少包含 size 字节。
 * @param size 中断发送字节数，必须大于零。
 * @retval DEV_UART_OK 发送请求已被底层接受。
 * @retval DEV_UART_BUSY 物理 UART 正忙。
 * @retval DEV_UART_ERROR 参数或实例状态非法。
 * @pre pdata 必须保持有效直到发送完成或错误回调触发。
 * @note 不分配内存；完成回调负责恢复发送状态。
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
 * @brief 转发 DMA 接收请求到 Platform UART。
 * @param uart_h 链路虚拟 UART 句柄，必须非 NULL。
 * @param pdata DMA 接收缓冲区，可写且容量至少为 size 字节。
 * @param size DMA 接收字节数，必须大于零。
 * @retval DEV_UART_OK DMA 接收已启动。
 * @retval DEV_UART_BUSY 物理 UART 或 DMA 正忙。
 * @retval DEV_UART_ERROR 参数、实例或 DMA 配置非法。
 * @pre pdata 必须位于 DMA 可访问内存，并保持有效直到完成或错误回调。
 * @note 不分配内存；链路默认使用单字节 IT 接收，此入口仅用于满足虚拟 UART 契约。
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
 * @brief 转发 DMA 发送请求到 Platform UART。
 * @param uart_h 链路虚拟 UART 句柄，必须非 NULL。
 * @param pdata 待发送数据，只读且至少包含 size 字节。
 * @param size DMA 发送字节数，必须大于零。
 * @retval DEV_UART_OK DMA 发送已启动。
 * @retval DEV_UART_BUSY 物理 UART 或 DMA 正忙。
 * @retval DEV_UART_ERROR 参数、实例或 DMA 配置非法。
 * @pre pdata 必须位于 DMA 可访问内存，并保持有效直到 TX 完成回调。
 * @note 不分配内存；JustFloat 发送路径依赖此所有权规则，完成前不得覆盖发送缓冲。
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
 * @param link_h 链路对象，必须非 NULL 且已初始化。
 * @pre 可在 VofaTask 或 USART1 RX 完成/错误 ISR 中调用，不得阻塞。
 * @post 底层接受单字节 IT 接收时 rx_rearm_pending 清零；失败时保持 true，由任务稍后重试。
 * @note 不分配内存；重挂只使用 link_h->rx_byte，完成回调会将新字节写入环形缓冲。
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
 * @param uart_h 触发回调的物理 UART 句柄；本函数不使用。
 * @param context 初始化时注册的 app_uart_link_t 指针，必须非 NULL。
 * @pre 由物理 UART 的 TX 完成中断同步调用，context 生命周期覆盖链路。
 * @post 虚拟 telemetry_view 的发送完成状态已推进，允许 JustFloat 接受下一帧。
 * @note ISR 上下文；不得阻塞、解析命令或分配内存。
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
 * @param uart_h 触发回调的物理 UART 句柄；本函数不使用。
 * @param context 初始化时注册的 app_uart_link_t 指针，必须非 NULL。
 * @pre 由 USART1 RX 完成 ISR 同步调用，且 rx_byte 已收到一个完整字节。
 * @post 字节写入 RX 环形缓冲并将 rx_head 前移；缓冲满时丢弃该字节并递增 rx_overflow_count。
 * @post 无论如何都尝试重新挂接下一字节接收，失败则由 VofaTask 重试。
 * @note ISR 上下文；这是环形缓冲唯一生产者，消费者只能是 VofaTask，不得在此解析命令。
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
 * @brief 处理 UART 错误并请求恢复接收。
 * @param uart_h 触发回调的物理 UART 句柄；本函数不使用。
 * @param context 初始化时注册的 app_uart_link_t 指针，必须非 NULL。
 * @pre 由物理 UART 错误回调同步调用，context 生命周期覆盖链路。
 * @post uart_error_count 递增，rx_rearm_pending 置 true；当前未完成命令的字节保留在环形缓冲中。
 * @note ISR 上下文；不直接重挂接收、不阻塞、不分配内存，恢复动作延后到 VofaTask。
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
 * @param link_h 链路对象，必须非 NULL。
 * @param line 可写 NUL 结尾命令行；解析器会原地切分，调用后内容不再有效。
 * @pre 仅由 App_UART_LinkProcessCommands 在 VofaTask 上下文调用。
 * @post 语法错误置 FORMAT_ERROR；回调拒绝或缺失置 REJECTED；回调接受置 ACCEPTED，并更新对应计数。
 * @note 回调同步执行且可能修改 EKF 参数；本函数不发送 ASCII 应答、不分配内存。
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
 * @param link_h 待初始化的静态链路对象；函数会先清零全部字段。
 * @param name 链路对象名，NUL 结尾，供 UART/JustFloat 诊断使用；必须非 NULL，空字符串会按名称接受。
 * @param command_handler 参数命令处理回调，必须非 NULL；链路不复制其代码或上下文。
 * @param command_context 传给 command_handler 的上下文，可为 NULL。
 * @retval PLATFORM_STATUS_OK 虚拟 UART、物理 UART、回调和 JustFloat 已装配。
 * @retval PLATFORM_STATUS_BUSY 物理 UART 实例的回调槽已被其他所有者占用。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 必需指针为空、UART 实例非法或配置不满足底层要求。
 * @retval PLATFORM_STATUS_ERROR 物理 UART 端口初始化失败。
 * @pre 已初始化 USART1 外设；调度器尚未启动或不希望并发访问该链路。
 * @pre link_h、name 和回调上下文的生命周期覆盖链路完整使用期；link_h 由函数清零并接管，通常使用静态存储。
 * @post 成功时链路拥有物理 UART 的唯一回调槽，但 RX 中断尚未启动，必须再调用 App_UART_LinkStart。
 * @note 不分配动态内存；失败后链路不可继续使用，调用方应停止初始化流程。
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
 * @param link_h 已由 App_UART_LinkInit 成功初始化的链路实例；非 NULL 且 initialized 为 true。
 * @retval PLATFORM_STATUS_OK 单字节 IT 接收已挂载，rx_rearm_pending 清零。
 * @retval PLATFORM_STATUS_BUSY 物理 UART 忙；已在链路中标记待重挂，由命令处理阶段重试。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT link_h 为 NULL 或尚未完成初始化。
 * @retval PLATFORM_STATUS_ERROR 物理 UART 返回非忙错误或实例状态非法。
 * @pre 只允许在链路初始化完成后、任务开始处理命令前调用。
 * @post 返回 BUSY 或 ERROR 时链路仍保留重挂意图，不会静默丢弃接收恢复。
 * @note 不分配内存；在任务上下文调用，底层 IT 启动不得在 RX ISR 内重入。
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
 * @param link_h 链路对象；NULL 或未初始化时函数不执行任何操作。
 * @param now_ms 当前单调毫秒时间戳；当前未参与解析或节流，仅保留为任务时间戳接口。
 * @pre 只允许 VofaTask 调用；RX ISR 只生产字节，不能与消费者同时解析同一命令行。
 * @post 已消费环形缓冲中的全部字节；完整 `\n` 行会同步调用命令回调，`\r` 被忽略。
 * @post 缓冲满时清空已收集内容，对每个被丢弃字节递增 rejected_commands 并置 OVERFLOW；后续字节从行首重新收集。
 * @post 重挂失败会在下次调用重试；后续完整行可能把 last_command_status 覆盖为 ACCEPTED 或 REJECTED。
 * @note 不分配内存；回调在调用者上下文执行，环形缓冲采用单生产者 ISR、单消费者任务模型。
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

/**
 * @brief 非阻塞发送一帧 32 通道 JustFloat。
 * @param link_h 已初始化的链路对象，必须非 NULL。
 * @param sample 待发送的一致样本；只读，调用方保留所有权，函数返回后即可释放或复用。
 * @retval DEV_TELEMETRY_OK 整帧已复制到 JustFloat 发送缓冲并由 DMA 接管。
 * @retval DEV_TELEMETRY_BUSY 上一帧仍在发送；本帧整帧丢弃，不等待。
 * @retval DEV_TELEMETRY_ERROR 样本、通道掩码或 UART 后端状态非法，整帧未发送。
 * @pre link_h 与 sample 非 NULL，且链路已完成初始化和接收启动。
 * @post 本函数不阻塞；发送缓冲和 DMA 数据的所有权在完成回调前归 JustFloat 发送器所有。
 * @note 只允许 VofaTask 调用；调试构建对空指针 assert，不分配内存。
 */
dev_telemetry_status_t App_UART_LinkSendTelemetry(
  app_uart_link_t *link_h,
  const telemetry_sample_t *sample
) {
  assert(link_h && sample);
  return Dev_Telemetry_Base_Send(&link_h->telemetry.base, sample);
}

/**
 * @brief 复制链路状态。
 * @param link_h 链路对象，必须非 NULL；计数器可能正被 RX/TX ISR 更新。
 * @param status 输出快照；函数完整覆盖其字段，所有权仍归调用方。
 * @pre link_h 和 status 非 NULL；status 可写且不与链路对象重叠。
 * @post last_command_status、命令计数、RX 溢出/错误计数和 JustFloat 状态均已复制。
 * @note 各计数为独立读取，不保证跨字段的全局原子快照；不分配内存，不访问硬件寄存器。
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
