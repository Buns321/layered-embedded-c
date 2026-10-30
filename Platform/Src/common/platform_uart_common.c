/**
 * @file platform_uart_common.c
 * @brief 与芯片系列无关的 UART 参数校验、错误管理和回调分发。
 * @ingroup platform
 */

#include "platform_error_internal.h"
#include "platform_uart_backend.h"
#include <stddef.h>

static volatile uint32_t uart_error[PLATFORM_UART_INSTANCE_COUNT];
static platform_uart_callback_binding_t uart_cb[PLATFORM_UART_INSTANCE_COUNT];
static bool uart_cb_registered[PLATFORM_UART_INSTANCE_COUNT];

/**
 * @brief 把 HAL UART 结果转换为公共状态并记录错误。
 * @param uart 已通过后端校验的 UART 实例；用于选择软件错误槽位。
 * @param status 后端返回的公共操作状态。
 * @return 原样返回 status，便于调用方直接转交。
 * @pre uart 必须小于 PLATFORM_UART_INSTANCE_COUNT，否则会越界访问错误数组。
 * @post status 非 OK 时，将 platform_error_from_status(status) 写入该实例错误快照；
 *       成功时不修改快照。
 * @note 内部辅助函数；不分配内存、不阻塞，快照并发限制由 platform_error_record() 决定。
 */
static platform_status_t platform_uart_finish(
  platform_uart_instance_t uart,
  platform_status_t status
) {
  if (status != PLATFORM_STATUS_OK) {
    platform_error_record(
      &uart_error[uart],
      platform_error_from_status(status)
    );
  }
  return status;
}

/**
 * @brief 校验 UART 实例、缓冲区和长度参数。
 * @param uart 待校验的 UART 实例；必须启用并映射到 HAL 句柄。
 * @param data 数据缓冲区指针；必须非 NULL，访问权限由传输方向决定。
 * @param size 传输字节数；必须大于 0。
 * @retval true 实例、缓冲区和长度均满足要求。
 * @retval false 任一字段越界或 data 为 NULL。
 * @post 纯查询；不访问 UART、不修改输入对象或模块状态。
 * @note 内部辅助函数；不分配内存，阻塞方向由调用接口决定。
 */
static bool platform_uart_validate(
  platform_uart_instance_t uart,
  const void *data,
  uint16_t size
) {
  return (uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT &&
         platform_uart_backend_is_valid(uart) &&
         data != NULL &&
         size != 0U;
}

/**
 * @brief 构造 UART 非法参数状态。
 * @param uart 待报告的 UART 实例；允许越界值。
 * @return 始终返回 PLATFORM_STATUS_INVALID_ARGUMENT。
 * @post uart 位于槽位范围内时记录 PLATFORM_ERROR_INVALID_ARGUMENT；越界值不访问数组。
 * @note 内部辅助函数；不访问 UART 硬件、不分配内存、不阻塞。
 */
static platform_status_t platform_uart_invalid(
  platform_uart_instance_t uart
) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_record(
      &uart_error[uart],
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
  }
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

/**
 * @brief 把 UART 完成、错误和中止事件分发给已注册回调。
 * @param uart UART 实例；非法或未注册时无操作。
 * @param type 事件类型；ERROR 事件附带后端当前错误位，其他事件错误位为 NONE。
 * @post 已注册且 callback 非 NULL 时同步调用一次；事件对象只在回调期间有效。
 * @warning 由 HAL 中断回调调用，执行上下文为中断；不得阻塞或分配内存。
 * @note 不保留事件或缓冲区指针；一个物理实例只有一个回调所有者。
 */
void platform_uart_dispatch_event(
  platform_uart_instance_t uart,
  platform_uart_event_type_t type
) {
  if (
    (uint32_t)uart >= PLATFORM_UART_INSTANCE_COUNT ||
    !uart_cb_registered[uart]
  ) {
    return;
  }

  platform_uart_event_t event = {
    .type = type,
    .error = type == PLATFORM_UART_EVENT_ERROR
               ? platform_uart_backend_get_error(uart)
               : PLATFORM_UART_ERROR_NONE,
  };
  if (uart_cb[uart].callback != NULL) {
    uart_cb[uart].callback(uart, &event, uart_cb[uart].context);
  }
}

/**
 * @brief 阻塞接收。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param pdata 接收缓冲区；不得为 NULL，调用期间可写且容量至少为 size 字节。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @param timeout 超时时间，单位 ms；PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 已接收 size 字节。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、缓冲区或长度非法；不访问 UART。
 * @retval PLATFORM_STATUS_BUSY 实例正忙。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内未收满 size 字节。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件报告接收失败。
 * @pre 对应 UART 的 MX_USARTx_UART_Init() 已完成。
 * @post 成功时 pdata 前 size 字节已填充；失败时内容可能部分更新，不得假定完整。
 * @warning 只能在任务上下文调用；同一实例的事务必须串行化，pdata 必须覆盖整个调用。
 * @note 不分配内存、不自动重新挂起接收；失败时记录对应 PLATFORM_ERROR_* 位。
 */
platform_status_t Platform_UART_Receive(
  platform_uart_instance_t uart,
  uint8_t *pdata,
  uint16_t size,
  uint32_t timeout
) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_reset(&uart_error[uart]);
  }
  if (!platform_uart_validate(uart, pdata, size)) {
    return platform_uart_invalid(uart);
  }
  return platform_uart_finish(
    uart,
    platform_uart_backend_receive(uart, pdata, size, timeout)
  );
}

/**
 * @brief 阻塞发送。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param pdata 发送缓冲区；不得为 NULL，调用期间只读且至少包含 size 字节。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @param timeout 超时时间，单位 ms；PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 已发送 size 字节。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、缓冲区或长度非法；不访问 UART。
 * @retval PLATFORM_STATUS_BUSY 实例正忙。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内未完成发送。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件报告发送失败。
 * @pre 对应 UART 的 MX_USARTx_UART_Init() 已完成。
 * @post 成功时 size 字节已发送；本层不复制或释放 pdata。
 * @warning 只能在任务上下文调用；同一实例的事务必须串行化，pdata 必须覆盖整个调用。
 * @note 不分配内存；失败时记录对应 PLATFORM_ERROR_* 位。
 */
platform_status_t Platform_UART_Transmit(
  platform_uart_instance_t uart,
  const uint8_t *pdata,
  uint16_t size,
  uint32_t timeout
) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_reset(&uart_error[uart]);
  }
  if (!platform_uart_validate(uart, pdata, size)) {
    return platform_uart_invalid(uart);
  }
  return platform_uart_finish(
    uart,
    platform_uart_backend_transmit(uart, pdata, size, timeout)
  );
}

/**
 * @brief 中断接收。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param pdata 接收缓冲区；不得为 NULL，容量至少为 size 字节。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受中断接收请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、缓冲区或长度非法；不访问 UART。
 * @retval PLATFORM_STATUS_BUSY 实例已有活动接收或 HAL 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件无法启动请求。
 * @pre MX_USARTx_UART_Init() 和 NVIC 已完成配置；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受；完成、错误或中止通过回调异步通知。
 * @warning pdata 必须在 RX 完成、错误或中止前保持有效且不得并发访问；回调在中断上下文执行。
 * @note Platform 不复制缓冲区，也不自动重新挂起接收；重新挂接收由设备驱动负责。
 */
platform_status_t Platform_UART_ReceiveIT(
  platform_uart_instance_t uart,
  uint8_t *pdata,
  uint16_t size
) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_reset(&uart_error[uart]);
  }
  if (!platform_uart_validate(uart, pdata, size)) {
    return platform_uart_invalid(uart);
  }
  return platform_uart_finish(
    uart,
    platform_uart_backend_receive_it(uart, pdata, size)
  );
}

/**
 * @brief 中断发送。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param pdata 发送缓冲区；不得为 NULL，至少包含 size 字节。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受中断发送请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、缓冲区或长度非法；不访问 UART。
 * @retval PLATFORM_STATUS_BUSY 实例已有活动发送或 HAL 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件无法启动请求。
 * @pre MX_USARTx_UART_Init() 和 NVIC 已完成配置；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受；完成、错误或中止通过回调异步通知。
 * @warning pdata 必须在 TX 完成、错误或中止前保持有效且内容不变；回调在中断上下文执行。
 * @note Platform 不复制缓冲区，也不自动重新启动发送。
 */
platform_status_t Platform_UART_TransmitIT(
  platform_uart_instance_t uart,
  const uint8_t *pdata,
  uint16_t size
) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_reset(&uart_error[uart]);
  }
  if (!platform_uart_validate(uart, pdata, size)) {
    return platform_uart_invalid(uart);
  }
  return platform_uart_finish(
    uart,
    platform_uart_backend_transmit_it(uart, pdata, size)
  );
}

/**
 * @brief DMA 接收。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param pdata 接收缓冲区；不得为 NULL，容量至少为 size 字节。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受 DMA 接收请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、缓冲区或长度非法；不访问 UART。
 * @retval PLATFORM_STATUS_BUSY 实例已有活动接收或 DMA 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或 DMA 无法启动请求。
 * @pre MX_USARTx_UART_Init() 和 CubeMX RX DMA 配置已完成；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受；完成、错误或中止通过回调异步通知。
 * @warning pdata 必须在 RX 完成、错误或中止前保持有效且不得并发访问；半完成时只能访问已完成的那一半。
 * @note Platform 不复制缓冲区，也不自动重新挂起接收；重新挂接收由设备驱动负责。
 */
platform_status_t Platform_UART_ReceiveDMA(
  platform_uart_instance_t uart,
  uint8_t *pdata,
  uint16_t size
) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_reset(&uart_error[uart]);
  }
  if (!platform_uart_validate(uart, pdata, size)) {
    return platform_uart_invalid(uart);
  }
  return platform_uart_finish(
    uart,
    platform_uart_backend_receive_dma(uart, pdata, size)
  );
}

/**
 * @brief DMA 发送。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param pdata 发送缓冲区；不得为 NULL，至少包含 size 字节。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受 DMA 发送请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、缓冲区或长度非法；不访问 UART。
 * @retval PLATFORM_STATUS_BUSY 实例已有活动发送或 DMA 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或 DMA 无法启动请求。
 * @pre MX_USARTx_UART_Init() 和 CubeMX TX DMA 配置已完成；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受；完成、错误或中止通过回调异步通知。
 * @warning pdata 必须在 TX 完成、错误或中止前保持有效且内容不变；回调在中断上下文执行。
 * @note Platform 不复制缓冲区，也不自动重新启动发送。
 */
platform_status_t Platform_UART_TransmitDMA(
  platform_uart_instance_t uart,
  const uint8_t *pdata,
  uint16_t size
) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_reset(&uart_error[uart]);
  }
  if (!platform_uart_validate(uart, pdata, size)) {
    return platform_uart_invalid(uart);
  }
  return platform_uart_finish(
    uart,
    platform_uart_backend_transmit_dma(uart, pdata, size)
  );
}

/**
 * @brief 中止接收，清除 UART 硬件错误标志和 HAL 错误码。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @retval PLATFORM_STATUS_OK 接收中止完成，RX 标志和 HAL 错误码已清除。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界或未启用；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告实例忙。
 * @retval PLATFORM_STATUS_ERROR HAL 中止失败；不得假定接收已经停止。
 * @pre 调用方已启动接收或确认实例可接受中止请求，且同一实例没有并发的中止操作。
 * @post 成功时可显式重新启动接收；失败时保留 HAL 报告的硬件状态。
 * @warning 只能在任务上下文调用；内部可能同步等待 HAL 完成中止。
 * @note 调用开始时清除软件错误快照；不释放接收缓冲区。
 */
platform_status_t Platform_UART_AbortReceive(
  platform_uart_instance_t uart
) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_reset(&uart_error[uart]);
  }
  if (!platform_uart_backend_is_valid(uart)) {
    return platform_uart_invalid(uart);
  }
  return platform_uart_finish(
    uart,
    platform_uart_backend_abort_receive(uart)
  );
}

/**
 * @brief 中止发送并清除 HAL 错误码。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @retval PLATFORM_STATUS_OK 发送中止完成，HAL 错误码已清除。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界或未启用；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告实例忙。
 * @retval PLATFORM_STATUS_ERROR HAL 中止失败；不得假定发送已经停止。
 * @pre 调用方已启动发送或确认实例可接受中止请求，且同一实例没有并发的中止操作。
 * @post 成功时可显式重新启动发送；失败时保留 HAL 报告的硬件状态。
 * @warning 只能在任务上下文调用；内部可能同步等待 HAL 完成中止。
 * @note 调用开始时清除软件错误快照；不释放发送缓冲区。
 */
platform_status_t Platform_UART_AbortTransmit(
  platform_uart_instance_t uart
) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_reset(&uart_error[uart]);
  }
  if (!platform_uart_backend_is_valid(uart)) {
    return platform_uart_invalid(uart);
  }
  return platform_uart_finish(
    uart,
    platform_uart_backend_abort_transmit(uart)
  );
}

/**
 * @brief 中止发送和接收，并清除 UART 错误状态。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @retval PLATFORM_STATUS_OK 双向传输已中止，RX 标志和 HAL 错误码已清除。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界或未启用；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告实例忙。
 * @retval PLATFORM_STATUS_ERROR HAL 中止失败；不得假定发送和接收都已停止。
 * @pre 调用方已启动需要中止的传输，且同一实例没有并发的中止或回调状态修改。
 * @post 成功时发送和接收均可显式重新启动；失败时保留 HAL 报告的硬件状态。
 * @warning 只能在任务上下文调用；内部可能同步等待 HAL 完成中止。
 * @note 调用开始时清除软件错误快照；不释放任何缓冲区。
 */
platform_status_t Platform_UART_Abort(platform_uart_instance_t uart) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_reset(&uart_error[uart]);
  }
  if (!platform_uart_backend_is_valid(uart)) {
    return platform_uart_invalid(uart);
  }
  return platform_uart_finish(uart, platform_uart_backend_abort(uart));
}

/**
 * @brief 兼容旧接口：中止接收并清除错误状态。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @retval PLATFORM_STATUS_OK 接收中止完成，RX 标志和 HAL 错误码已清除。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界或未启用；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告实例忙。
 * @retval PLATFORM_STATUS_ERROR HAL 中止失败；不得假定接收已经停止。
 * @pre 调用方已启动接收或确认实例可接受中止请求，且同一实例没有并发的中止操作。
 * @post 直接转交接收中止操作，不执行额外的缓冲区清理；新代码不得依赖该别名。
 * @warning 该接口已弃用，新代码必须使用 Platform_UART_AbortReceive()；只能在任务上下文
 *          调用，并遵守该接口的并发限制。
 * @note 保留该接口仅为兼容旧调用，不分配内存。
 */
platform_status_t Platform_UART_ClearBuffer(
  platform_uart_instance_t uart
) {
  return Platform_UART_AbortReceive(uart);
}

/**
 * @brief 读取最近一次 HAL UART 错误快照。
 * @param uart UART 实例；当前仅有效枚举值可读取后端错误。
 * @retval PLATFORM_ERROR_INVALID_ARGUMENT uart 越界。
 * @post 合法实例返回软件错误位与 HAL 错误映射的按位或；非法实例不访问快照。
 * @note 只读，不清除软件或 HAL 错误；不停止传输、不修改缓冲区，不分配内存。
 */
uint32_t Platform_UART_GetLastError(platform_uart_instance_t uart) {
  if ((uint32_t)uart >= PLATFORM_UART_INSTANCE_COUNT) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return uart_error[uart] | platform_uart_backend_get_error(uart);
}

/**
 * @brief 注册 UART 回调。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param callbacks 待复制的回调集合；不得为 NULL，callback 成员可为 NULL 以禁用通知。
 * @pre 必须在启动 IT/DMA 传输前调用，且一个实例只能有一个回调所有者。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该实例已注册回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或回调集合非法。
 * @post 成功时复制 callback 和 context，后续完成、错误或中止事件调用该回调。
 * @warning 回调在中断上下文执行；事件对象只在回调期间有效，不得阻塞、分配内存或保存指针。
 * @note 每个实例仅允许成功注册一次；context 的生命周期由调用方维护。
 */
platform_status_t Platform_UART_RegisterCallback(
  platform_uart_instance_t uart,
  const platform_uart_callback_binding_t *callbacks
) {
  if (
    (uint32_t)uart >= PLATFORM_UART_INSTANCE_COUNT ||
    callbacks == NULL ||
    !platform_uart_backend_is_valid(uart)
  ) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (uart_cb_registered[uart]) {
    return PLATFORM_STATUS_BUSY;
  }

  uart_cb[uart] = *callbacks;
  uart_cb_registered[uart] = true;
  return PLATFORM_STATUS_OK;
}
