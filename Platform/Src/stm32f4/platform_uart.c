/**
 * @file platform_uart.c
 * @brief STM32F4 公共 UART 实现的 HAL 后端。
 * @ingroup platform
 */

#include "platform_hal_internal.h"
#include "platform_uart_backend.h"
#include "stm32f4xx_hal.h"
#include "usart.h"

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

static void platform_uart_clear_rx_flags(UART_HandleTypeDef *uart_h) {
  __HAL_UART_CLEAR_OREFLAG(uart_h);
  __HAL_UART_CLEAR_FEFLAG(uart_h);
  __HAL_UART_CLEAR_NEFLAG(uart_h);
  __HAL_UART_CLEAR_PEFLAG(uart_h);
}

bool platform_uart_backend_is_valid(platform_uart_instance_t uart) {
  return (uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT &&
         platform_uart_to_hal(uart) != NULL;
}

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

uint32_t platform_uart_backend_get_error(platform_uart_instance_t uart) {
  UART_HandleTypeDef *uart_h = platform_uart_to_hal(uart);
  return uart_h == NULL
           ? PLATFORM_UART_ERROR_UNKNOWN
           : platform_uart_map_error(uart_h->ErrorCode);
}

static void platform_uart_dispatch_hal_event(
  UART_HandleTypeDef *uart_h,
  platform_uart_event_type_t type
) {
  platform_uart_instance_t uart;
  if (platform_hal_to_uart(uart_h, &uart)) {
    platform_uart_dispatch_event(uart, type);
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart_h) {
  platform_uart_dispatch_hal_event(uart_h, PLATFORM_UART_EVENT_TX_COMPLETE);
}

void HAL_UART_TxHalfCpltCallback(UART_HandleTypeDef *uart_h) {
  platform_uart_dispatch_hal_event(uart_h, PLATFORM_UART_EVENT_TX_HALF_COMPLETE);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart_h) {
  platform_uart_dispatch_hal_event(uart_h, PLATFORM_UART_EVENT_RX_COMPLETE);
}

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *uart_h) {
  platform_uart_dispatch_hal_event(uart_h, PLATFORM_UART_EVENT_RX_HALF_COMPLETE);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart_h) {
  platform_uart_dispatch_hal_event(uart_h, PLATFORM_UART_EVENT_ERROR);
}

void HAL_UART_AbortCpltCallback(UART_HandleTypeDef *uart_h) {
  platform_uart_dispatch_hal_event(uart_h, PLATFORM_UART_EVENT_ABORT_COMPLETE);
}

void HAL_UART_AbortTransmitCpltCallback(UART_HandleTypeDef *uart_h) {
  platform_uart_dispatch_hal_event(
    uart_h,
    PLATFORM_UART_EVENT_ABORT_TX_COMPLETE
  );
}

void HAL_UART_AbortReceiveCpltCallback(UART_HandleTypeDef *uart_h) {
  platform_uart_dispatch_hal_event(
    uart_h,
    PLATFORM_UART_EVENT_ABORT_RX_COMPLETE
  );
}
