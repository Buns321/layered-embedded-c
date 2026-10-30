/**
 * @file platform_uart.c
 * @brief STM32F4 公共 UART 实现的 HAL 后端。
 * @ingroup platform
 */

#include "platform_hal_internal.h"
#include "platform_uart_backend.h"
#include "stm32f4xx_hal.h"
#include "usart.h"

/**
 * @brief 将平台 UART 实例映射为 CubeMX 生成的 HAL 句柄。
 * @param uart UART 实例编号；应为 platform_uart.h 中由 PLATFORM_ENABLE_UARTx 启用的枚举值。
 * @return 对应句柄指针；未启用或非法实例返回 NULL。
 * @pre 对启用实例，CubeMX 必须已生成并初始化对应 UART_HandleTypeDef 全局句柄。
 * @post 成功时返回指向全局 HAL 句柄的长期有效指针；函数不改句柄内容，也不操作硬件。
 * @note 仅在本编译单元内调用；映射表为编译期静态数据，可在任务或 ISR 上下文只读调用。
 */
static UART_HandleTypeDef *platform_uart_to_hal(
  platform_uart_instance_t uart
) {
  switch (uart) {
#if PLATFORM_ENABLE_UART1
    case PLATFORM_UART_1: return &huart1;
#endif
#if PLATFORM_ENABLE_UART2
    case PLATFORM_UART_2: return &huart2;
#endif
#if PLATFORM_ENABLE_UART3
    case PLATFORM_UART_3: return &huart3;
#endif
#if PLATFORM_ENABLE_UART4
    case PLATFORM_UART_4: return &huart4;
#endif
#if PLATFORM_ENABLE_UART5
    case PLATFORM_UART_5: return &huart5;
#endif
#if PLATFORM_ENABLE_UART6
    case PLATFORM_UART_6: return &huart6;
#endif
    default: return NULL;
  }
}

/**
 * @brief 将 HAL UART 句柄识别为平台实例编号。
 * @param uart_h HAL UART 句柄；可为 NULL，NULL 时不匹配。
 * @param uart 输出实例编号；可为 NULL，NULL 时不匹配；成功时写入启用实例。
 * @retval true uart_h 与某个已启用实例的 CubeMX 句柄匹配，且 *uart 已写入。
 * @retval false 任一指针为 NULL、句柄不属于本板启用实例，或实例未启用。
 * @post 返回 false 时不写 *uart；成功时返回的实例可直接用于后端映射。
 * @note 仅在本编译单元内调用；只比较全局句柄地址，不访问硬件，可在 ISR 上下文调用。
 */
static bool platform_hal_to_uart(
  UART_HandleTypeDef *uart_h,
  platform_uart_instance_t *uart
) {
  if (uart_h == NULL || uart == NULL) {
    return false;
  }

#if PLATFORM_ENABLE_UART1
  if (uart_h == &huart1) {
    *uart = PLATFORM_UART_1;
    return true;
  }
#endif
#if PLATFORM_ENABLE_UART2
  if (uart_h == &huart2) {
    *uart = PLATFORM_UART_2;
    return true;
  }
#endif
#if PLATFORM_ENABLE_UART3
  if (uart_h == &huart3) {
    *uart = PLATFORM_UART_3;
    return true;
  }
#endif
#if PLATFORM_ENABLE_UART4
  if (uart_h == &huart4) {
    *uart = PLATFORM_UART_4;
    return true;
  }
#endif
#if PLATFORM_ENABLE_UART5
  if (uart_h == &huart5) {
    *uart = PLATFORM_UART_5;
    return true;
  }
#endif
#if PLATFORM_ENABLE_UART6
  if (uart_h == &huart6) {
    *uart = PLATFORM_UART_6;
    return true;
  }
#endif
  return false;
}

/**
 * @brief 将 HAL UART 错误位映射为平台 UART 错误位。
 * @param hal_error HAL UART 错误码位掩码；允许同时置位多个已知或未知错误。
 * @retval PLATFORM_UART_ERROR_NONE hal_error 为 0 时。
 * @post 不修改 HAL 错误寄存器、句柄错误码或传输状态。
 * @note 仅在本编译单元内调用；纯位映射，可在任务或 ISR 上下文调用。
 */
static uint32_t platform_uart_map_error(uint32_t hal_error) {
  uint32_t error = PLATFORM_UART_ERROR_NONE;

  if ((hal_error & HAL_UART_ERROR_PE) != 0U) {
    error |= PLATFORM_UART_ERROR_PARITY;
  }
  if ((hal_error & HAL_UART_ERROR_NE) != 0U) {
    error |= PLATFORM_UART_ERROR_NOISE;
  }
  if ((hal_error & HAL_UART_ERROR_FE) != 0U) {
    error |= PLATFORM_UART_ERROR_FRAME;
  }
  if ((hal_error & HAL_UART_ERROR_ORE) != 0U) {
    error |= PLATFORM_UART_ERROR_OVERRUN;
  }
  if ((hal_error & HAL_UART_ERROR_DMA) != 0U) {
    error |= PLATFORM_UART_ERROR_DMA;
  }

  const uint32_t known_errors =
    HAL_UART_ERROR_PE |
    HAL_UART_ERROR_NE |
    HAL_UART_ERROR_FE |
    HAL_UART_ERROR_ORE |
    HAL_UART_ERROR_DMA;
  if ((hal_error & ~known_errors) != 0U) {
    error |= PLATFORM_UART_ERROR_UNKNOWN;
  }
  return error;
}

/**
 * @brief 清除 UART 接收溢出、帧、噪声和奇偶校验标志。
 * @param uart_h 已初始化的 UART HAL 句柄；不得为 NULL。
 * @pre 调用方已通过 HAL 中止接收，且没有并发的接收中断会重新置位这些标志。
 * @post ORE、FE、NE、PE 硬件标志被清除；不改变缓冲区、波特率或其他配置。
 * @note 仅在本编译单元内调用；直接写 UART 状态寄存器，必须在任务或受控上下文执行。
 */
static void platform_uart_clear_rx_flags(UART_HandleTypeDef *uart_h) {
  __HAL_UART_CLEAR_OREFLAG(uart_h);
  __HAL_UART_CLEAR_FEFLAG(uart_h);
  __HAL_UART_CLEAR_NEFLAG(uart_h);
  __HAL_UART_CLEAR_PEFLAG(uart_h);
}

/**
 * @brief 校验 UART 实例是否映射到已启用的 HAL 句柄。
 * @param uart UART 实例编号；应为 platform_uart.h 中由 PLATFORM_ENABLE_UARTx 启用的枚举值。
 * @retval true 实例在身份槽位范围内且存在对应的 CubeMX HAL 句柄。
 * @retval false 实例越界、未启用或没有对应的 HAL 句柄。
 * @post 不修改实例、HAL 句柄或硬件状态。
 * @note 只读静态映射，可在任务或中断上下文调用；不分配内存，不访问外设寄存器。
 */
bool platform_uart_backend_is_valid(platform_uart_instance_t uart) {
  return (uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT &&
         platform_uart_to_hal(uart) != NULL;
}

/**
 * @brief 以阻塞方式接收 UART 数据。
 * @param uart UART 实例编号；必须映射到已启用的 HAL 句柄。
 * @param data 接收缓冲区；不得为 NULL，调用期间保持可写且有效。
 * @param size 接收字节数，范围 1..65535。
 * @param timeout 超时时间，单位 ms；可为 PLATFORM_TIMEOUT_INFINITE。
 * @return 操作状态；HAL 结果映射为 OK、ERROR、BUSY、TIMEOUT 或实例非法时的 INVALID_ARGUMENT。
 * @pre CubeMX 已完成 UART 初始化；一次只允许一个阻塞事务使用该实例。
 * @post 成功返回时 data 的前 size 字节已接收；失败时已写入内容由 HAL 决定，不得假定完整。
 * @note 仅在任务上下文调用且会阻塞；Platform 不复制缓冲区，也不自动重新挂起接收。
 */
platform_status_t platform_uart_backend_receive(
  platform_uart_instance_t uart,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  UART_HandleTypeDef *uart_h = platform_uart_to_hal(uart);
  return uart_h == NULL
           ? PLATFORM_STATUS_INVALID_ARGUMENT
           : platform_from_hal_status(
               HAL_UART_Receive(uart_h, data, size, timeout)
             );
}

/**
 * @brief 以阻塞方式发送 UART 数据。
 * @param uart UART 实例编号；必须映射到已启用的 HAL 句柄。
 * @param data 发送缓冲区；不得为 NULL，调用期间保持只读且有效。
 * @param size 发送字节数，范围 1..65535。
 * @param timeout 超时时间，单位 ms；可为 PLATFORM_TIMEOUT_INFINITE。
 * @return 操作状态；HAL 结果映射为 OK、ERROR、BUSY、TIMEOUT 或实例非法时的 INVALID_ARGUMENT。
 * @pre CubeMX 已完成 UART 初始化；一次只允许一个阻塞事务使用该实例。
 * @post 成功返回表示发送完成；函数不复制或释放调用方缓冲区。
 * @note 仅在任务上下文调用且会阻塞；同一实例的并发访问必须由上层串行化。
 */
platform_status_t platform_uart_backend_transmit(
  platform_uart_instance_t uart,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  UART_HandleTypeDef *uart_h = platform_uart_to_hal(uart);
  return uart_h == NULL
           ? PLATFORM_STATUS_INVALID_ARGUMENT
           : platform_from_hal_status(
               HAL_UART_Transmit(uart_h, (uint8_t *)data, size, timeout)
             );
}

/**
 * @brief 启动 UART 中断接收。
 * @param uart UART 实例编号；必须映射到已启用的 HAL 句柄。
 * @param data 接收缓冲区；不得为 NULL，从启动成功到最终事件期间保持可写且有效。
 * @param size 接收字节数，范围 1..65535。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或 HAL 句柄不存在。
 * @pre 回调已注册，CubeMX 已配置 UART 中断和 NVIC；同一实例无其他活动接收。
 * @post 成功时实例进入忙碌状态，后续由完成、错误或中止事件结束本次接收。
 * @note 从任务上下文启动；回调在中断上下文执行，Platform 不复制缓冲区且不自动重新挂起接收。
 */
platform_status_t platform_uart_backend_receive_it(
  platform_uart_instance_t uart,
  uint8_t *data,
  uint16_t size
) {
  UART_HandleTypeDef *uart_h = platform_uart_to_hal(uart);
  return uart_h == NULL
           ? PLATFORM_STATUS_INVALID_ARGUMENT
           : platform_from_hal_status(
               HAL_UART_Receive_IT(uart_h, data, size)
             );
}

/**
 * @brief 启动 UART 中断发送。
 * @param uart UART 实例编号；必须映射到已启用的 HAL 句柄。
 * @param data 发送缓冲区；不得为 NULL，从启动成功到最终事件期间保持只读且有效。
 * @param size 发送字节数，范围 1..65535。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或 HAL 句柄不存在。
 * @pre 回调已注册，CubeMX 已配置 UART 中断和 NVIC；同一实例无其他活动发送。
 * @post 成功时实例进入忙碌状态，后续由 TX 完成、错误或中止事件结束本次发送。
 * @note 从任务上下文启动；回调在中断上下文执行，Platform 不复制缓冲区且不自动重启发送。
 */
platform_status_t platform_uart_backend_transmit_it(
  platform_uart_instance_t uart,
  const uint8_t *data,
  uint16_t size
) {
  UART_HandleTypeDef *uart_h = platform_uart_to_hal(uart);
  return uart_h == NULL
           ? PLATFORM_STATUS_INVALID_ARGUMENT
           : platform_from_hal_status(
               HAL_UART_Transmit_IT(uart_h, (uint8_t *)data, size)
             );
}

/**
 * @brief 启动 UART DMA 接收。
 * @param uart UART 实例编号；必须映射到已启用的 HAL 句柄。
 * @param data 接收缓冲区；不得为 NULL，从启动成功到最终事件期间保持可写且有效。
 * @param size 接收字节数，范围 1..65535。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或 HAL 句柄不存在。
 * @pre 回调已注册，CubeMX 已配置 UART RX DMA stream/channel 和 NVIC；缓冲区满足 DMA 可访问性。
 * @post 成功时 DMA 持有缓冲区地址，实例保持忙碌直到完成、错误或中止事件。
 * @note 从任务上下文启动；半完成事件只保证对应半区已写入，Platform 不复制缓冲区且不自动重挂。
 */
platform_status_t platform_uart_backend_receive_dma(
  platform_uart_instance_t uart,
  uint8_t *data,
  uint16_t size
) {
  UART_HandleTypeDef *uart_h = platform_uart_to_hal(uart);
  return uart_h == NULL
           ? PLATFORM_STATUS_INVALID_ARGUMENT
           : platform_from_hal_status(
               HAL_UART_Receive_DMA(uart_h, data, size)
             );
}

/**
 * @brief 启动 UART DMA 发送。
 * @param uart UART 实例编号；必须映射到已启用的 HAL 句柄。
 * @param data 发送缓冲区；不得为 NULL，从启动成功到最终事件期间保持只读且有效。
 * @param size 发送字节数，范围 1..65535。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或 HAL 句柄不存在。
 * @pre 回调已注册，CubeMX 已配置 UART TX DMA stream/channel 和 NVIC；缓冲区满足 DMA 可访问性。
 * @post 成功时 DMA 持有缓冲区地址，实例保持忙碌直到 TX 完成、错误或中止事件。
 * @note 从任务上下文启动；回调在中断上下文执行，Platform 不复制缓冲区且不自动重启发送。
 */
platform_status_t platform_uart_backend_transmit_dma(
  platform_uart_instance_t uart,
  const uint8_t *data,
  uint16_t size
) {
  UART_HandleTypeDef *uart_h = platform_uart_to_hal(uart);
  return uart_h == NULL
           ? PLATFORM_STATUS_INVALID_ARGUMENT
           : platform_from_hal_status(
               HAL_UART_Transmit_DMA(uart_h, (uint8_t *)data, size)
             );
}

/**
 * @brief 同步中止 UART 接收并清除接收错误状态。
 * @param uart UART 实例编号；必须映射到已启用的 HAL 句柄。
 * @retval PLATFORM_STATUS_OK 接收中止完成，RX 标志和 HAL 错误码已清除。
 * @pre 仅在任务上下文调用；不得与同一实例的接收启动或其他 abort 操作并发。
 * @post 成功时可显式重新启动接收；失败时保留 HAL 报告的状态，不清除错误标志。
 * @note 同步接口可能等待 HAL 完成中止，不要在中断上下文调用。
 */
platform_status_t platform_uart_backend_abort_receive(
  platform_uart_instance_t uart
) {
  UART_HandleTypeDef *uart_h = platform_uart_to_hal(uart);
  if (uart_h == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  HAL_StatusTypeDef status = HAL_UART_AbortReceive(uart_h);
  if (status == HAL_OK) {
    platform_uart_clear_rx_flags(uart_h);
    uart_h->ErrorCode = HAL_UART_ERROR_NONE;
  }
  return platform_from_hal_status(status);
}

/**
 * @brief 同步中止 UART 发送。
 * @param uart UART 实例编号；必须映射到已启用的 HAL 句柄。
 * @retval PLATFORM_STATUS_OK 发送中止完成，HAL 错误码已清除。
 * @pre 仅在任务上下文调用；不得与同一实例的发送启动或其他 abort 操作并发。
 * @post 成功时可显式重新启动发送；失败时保留 HAL 报告的状态。
 * @note 同步接口可能等待 HAL 完成中止，不要在中断上下文调用。
 */
platform_status_t platform_uart_backend_abort_transmit(
  platform_uart_instance_t uart
) {
  UART_HandleTypeDef *uart_h = platform_uart_to_hal(uart);
  if (uart_h == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  HAL_StatusTypeDef status = HAL_UART_AbortTransmit(uart_h);
  if (status == HAL_OK) {
    uart_h->ErrorCode = HAL_UART_ERROR_NONE;
  }
  return platform_from_hal_status(status);
}

/**
 * @brief 同步中止 UART 的发送和接收。
 * @param uart UART 实例编号；必须映射到已启用的 HAL 句柄。
 * @retval PLATFORM_STATUS_OK 双向传输已中止，RX 标志和 HAL 错误码已清除。
 * @pre 仅在任务上下文调用；不得与同一实例的传输启动或回调路径并发修改句柄状态。
 * @post 成功时发送和接收均可显式重新启动；失败时保留 HAL 报告的状态。
 * @note 同步接口可能等待 HAL 完成中止，不要在中断上下文调用。
 */
platform_status_t platform_uart_backend_abort(
  platform_uart_instance_t uart
) {
  UART_HandleTypeDef *uart_h = platform_uart_to_hal(uart);
  if (uart_h == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  HAL_StatusTypeDef status = HAL_UART_Abort(uart_h);
  if (status == HAL_OK) {
    platform_uart_clear_rx_flags(uart_h);
    uart_h->ErrorCode = HAL_UART_ERROR_NONE;
  }
  return platform_from_hal_status(status);
}

/**
 * @brief 读取 UART HAL 错误快照并转换为平台错误位。
 * @param uart UART 实例编号；应为已启用的平台枚举值。
 * @retval PLATFORM_UART_ERROR_NONE HAL 未记录已知错误。
 * @retval PLATFORM_UART_ERROR_UNKNOWN 实例非法/未映射，或 HAL 返回未识别错误位。
 * @post 只读 HAL 错误码，不清除错误，也不改变发送或接收状态。
 * @note 可在任务或中断上下文调用；与其他实例查询互不共享可变状态。
 */
uint32_t platform_uart_backend_get_error(platform_uart_instance_t uart) {
  UART_HandleTypeDef *uart_h = platform_uart_to_hal(uart);
  return uart_h == NULL
           ? PLATFORM_UART_ERROR_UNKNOWN
           : platform_uart_map_error(uart_h->ErrorCode);
}

/**
 * @brief 将 HAL UART 回调桥接到平台事件分发器。
 * @param uart_h HAL 传入的 UART 句柄；可为 NULL 或未启用的句柄，此时忽略事件。
 * @param type HAL 回调对应的平台事件类型。
 * @post 识别到启用实例时同步转发事件；否则不调用上层回调，也不修改硬件。
 * @note 由 HAL 中断回调调用；不得阻塞、分配内存或执行长耗时操作。
 */
static void platform_uart_dispatch_hal_event(
  UART_HandleTypeDef *uart_h,
  platform_uart_event_type_t type
) {
  platform_uart_instance_t uart;
  if (platform_hal_to_uart(uart_h, &uart)) {
    platform_uart_dispatch_event(uart, type);
  }
}

/**
 * @brief 处理 UART 中断/DMA 发送完成回调。
 * @param uart_h HAL 传入的 UART 句柄；可忽略 NULL 或未启用实例。
 * @post 向已注册平台回调发送 PLATFORM_UART_EVENT_TX_COMPLETE；不自动启动后续发送。
 * @note 在 HAL 中断上下文调用；必须保持短小且不可阻塞。
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart_h) {
  platform_uart_dispatch_hal_event(uart_h, PLATFORM_UART_EVENT_TX_COMPLETE);
}

/**
 * @brief 处理 UART DMA 半发送完成回调。
 * @param uart_h HAL 传入的 UART 句柄；可忽略 NULL 或未启用实例。
 * @post 向已注册平台回调发送 PLATFORM_UART_EVENT_TX_HALF_COMPLETE；发送缓冲区仍归调用方所有。
 * @note 仅在 DMA 发送配置产生该事件时由 HAL 中断上下文调用；不得阻塞或长耗时处理。
 */
void HAL_UART_TxHalfCpltCallback(UART_HandleTypeDef *uart_h) {
  platform_uart_dispatch_hal_event(uart_h, PLATFORM_UART_EVENT_TX_HALF_COMPLETE);
}

/**
 * @brief 处理 UART 中断/DMA 接收完成回调。
 * @param uart_h HAL 传入的 UART 句柄；可忽略 NULL 或未启用实例。
 * @post 向已注册平台回调发送 PLATFORM_UART_EVENT_RX_COMPLETE；不自动重新挂起接收。
 * @note 在 HAL 中断上下文调用；必须保持短小且不可阻塞。
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart_h) {
  platform_uart_dispatch_hal_event(uart_h, PLATFORM_UART_EVENT_RX_COMPLETE);
}

/**
 * @brief 处理 UART DMA 半接收完成回调。
 * @param uart_h HAL 传入的 UART 句柄；可忽略 NULL 或未启用实例。
 * @post 向已注册平台回调发送 PLATFORM_UART_EVENT_RX_HALF_COMPLETE；仅前半区数据已就绪。
 * @note 在 HAL DMA 中断上下文调用；不得阻塞、分配内存或读取尚未完成的半区。
 */
void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *uart_h) {
  platform_uart_dispatch_hal_event(uart_h, PLATFORM_UART_EVENT_RX_HALF_COMPLETE);
}

/**
 * @brief 处理 UART 错误回调并转发错误事件。
 * @param uart_h HAL 传入的 UART 句柄；可忽略 NULL 或未启用实例。
 * @post 平台事件分发器读取 HAL 错误快照并发送 PLATFORM_UART_EVENT_ERROR。
 * @note 在 HAL 中断上下文调用；不得阻塞、分配内存或执行长耗时处理。
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart_h) {
  platform_uart_dispatch_hal_event(uart_h, PLATFORM_UART_EVENT_ERROR);
}

/**
 * @brief 处理 UART 中断式双向中止完成回调。
 * @param uart_h HAL 传入的 UART 句柄；可忽略 NULL 或未启用实例。
 * @post 向已注册平台回调发送 PLATFORM_UART_EVENT_ABORT_COMPLETE；调用方可在事件后重新启动。
 * @note 在 HAL 中断上下文调用；不得阻塞、分配内存或执行长耗时处理。
 */
void HAL_UART_AbortCpltCallback(UART_HandleTypeDef *uart_h) {
  platform_uart_dispatch_hal_event(uart_h, PLATFORM_UART_EVENT_ABORT_COMPLETE);
}

/**
 * @brief 处理 UART 中断式发送中止完成回调。
 * @param uart_h HAL 传入的 UART 句柄；可忽略 NULL 或未启用实例。
 * @post 向已注册平台回调发送 PLATFORM_UART_EVENT_ABORT_TX_COMPLETE；接收状态不受影响。
 * @note 在 HAL 中断上下文调用；不得阻塞、分配内存或执行长耗时处理。
 */
void HAL_UART_AbortTransmitCpltCallback(UART_HandleTypeDef *uart_h) {
  platform_uart_dispatch_hal_event(
    uart_h,
    PLATFORM_UART_EVENT_ABORT_TX_COMPLETE
  );
}

/**
 * @brief 处理 UART 中断式接收中止完成回调。
 * @param uart_h HAL 传入的 UART 句柄；可忽略 NULL 或未启用实例。
 * @post 向已注册平台回调发送 PLATFORM_UART_EVENT_ABORT_RX_COMPLETE；发送状态不受影响。
 * @note 在 HAL 中断上下文调用；不得阻塞、分配内存或执行长耗时处理。
 */
void HAL_UART_AbortReceiveCpltCallback(UART_HandleTypeDef *uart_h) {
  platform_uart_dispatch_hal_event(
    uart_h,
    PLATFORM_UART_EVENT_ABORT_RX_COMPLETE
  );
}
