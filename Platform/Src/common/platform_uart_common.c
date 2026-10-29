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
 * @param uart UART 实例。
 * @param pdata 接收缓冲区。
 * @param size 字节数，必须大于 0。
 * @param timeout 超时时间（ms），可为 PLATFORM_TIMEOUT_INFINITE。
 * @pre 对应 UART 的 MX_USARTx_UART_Init() 已完成。
 * @return Platform 状态码。
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
 * @param uart UART 实例。
 * @param pdata 发送缓冲区，调用期间只读。
 * @param size 字节数，必须大于 0。
 * @param timeout 超时时间（ms），可为 PLATFORM_TIMEOUT_INFINITE。
 * @pre 对应 UART 的 MX_USARTx_UART_Init() 已完成。
 * @return Platform 状态码。
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
 * @note pdata 必须保持有效，直到 RX 完成或错误回调。
 * @note 重新挂接收由设备驱动负责，Platform 不自动重挂。
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
 * @note pdata 必须保持有效，直到 TX 完成或错误回调。
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
 * @note pdata 必须保持有效，直到 RX 完成或错误回调。
 * @note 半完成回调时只能访问已经完成的那一半数据。
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
 * @note pdata 必须保持有效，直到 TX 完成或错误回调。
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
 * @param uart UART 实例。
 * @retval PLATFORM_STATUS_OK 成功。
 * @retval PLATFORM_STATUS_ERROR 失败。
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
 * @param uart UART 实例。
 * @retval PLATFORM_STATUS_OK 成功。
 * @retval PLATFORM_STATUS_ERROR 失败。
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
 * @param uart UART 实例。
 * @retval PLATFORM_STATUS_OK 成功。
 * @retval PLATFORM_STATUS_ERROR 失败。
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
 * @deprecated 新代码应使用 Platform_UART_AbortReceive()。
 */
platform_status_t Platform_UART_ClearBuffer(
  platform_uart_instance_t uart
) {
  return Platform_UART_AbortReceive(uart);
}

/**
 * @brief 读取最近一次 HAL UART 错误快照。
 * @return platform_uart_error_t 位掩码。
 */
uint32_t Platform_UART_GetLastError(platform_uart_instance_t uart) {
  if ((uint32_t)uart >= PLATFORM_UART_INSTANCE_COUNT) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return uart_error[uart] | platform_uart_backend_get_error(uart);
}

/**
 * @brief 注册 UART 回调。
 * @param uart UART 实例。
 * @param callbacks 回调集合，内容会被复制。
 * @pre 必须在启动 IT/DMA 传输前调用，且一个实例只能有一个回调所有者。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该实例已注册回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或回调集合非法。
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
