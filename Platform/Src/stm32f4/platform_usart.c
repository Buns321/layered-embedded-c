/**
 * @file platform_usart.c
 * @brief STM32F4 HAL backend for the common UART implementation.
 */

#include "platform_hal_internal.h"
#include "platform_uart_backend.h"
#include "stm32f4xx_hal.h"
#include "usart.h"

static UART_HandleTypeDef *platform_uart_to_hal(
  Platform_UART_Instance uart
) {
  switch (uart) {
#if PLATFORM_ENABLE_UART1
    case PLATFORM_USART_1: return &huart1;
#endif
#if PLATFORM_ENABLE_UART2
    case PLATFORM_USART_2: return &huart2;
#endif
#if PLATFORM_ENABLE_UART3
    case PLATFORM_USART_3: return &huart3;
#endif
#if PLATFORM_ENABLE_UART4
    case PLATFORM_UART_4: return &huart4;
#endif
#if PLATFORM_ENABLE_UART5
    case PLATFORM_UART_5: return &huart5;
#endif
#if PLATFORM_ENABLE_UART6
    case PLATFORM_USART_6: return &huart6;
#endif
    default: return NULL;
  }
}

static bool platform_hal_to_uart(
  UART_HandleTypeDef *huart,
  Platform_UART_Instance *uart
) {
  if (huart == NULL || uart == NULL) {
    return false;
  }

#if PLATFORM_ENABLE_UART1
  if (huart == &huart1) {
    *uart = PLATFORM_USART_1;
    return true;
  }
#endif
#if PLATFORM_ENABLE_UART2
  if (huart == &huart2) {
    *uart = PLATFORM_USART_2;
    return true;
  }
#endif
#if PLATFORM_ENABLE_UART3
  if (huart == &huart3) {
    *uart = PLATFORM_USART_3;
    return true;
  }
#endif
#if PLATFORM_ENABLE_UART4
  if (huart == &huart4) {
    *uart = PLATFORM_UART_4;
    return true;
  }
#endif
#if PLATFORM_ENABLE_UART5
  if (huart == &huart5) {
    *uart = PLATFORM_UART_5;
    return true;
  }
#endif
#if PLATFORM_ENABLE_UART6
  if (huart == &huart6) {
    *uart = PLATFORM_USART_6;
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

static void platform_uart_clear_rx_flags(UART_HandleTypeDef *huart) {
  __HAL_UART_CLEAR_OREFLAG(huart);
  __HAL_UART_CLEAR_FEFLAG(huart);
  __HAL_UART_CLEAR_NEFLAG(huart);
  __HAL_UART_CLEAR_PEFLAG(huart);
}

bool platform_uart_backend_is_valid(Platform_UART_Instance uart) {
  return (uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT &&
         platform_uart_to_hal(uart) != NULL;
}

Platform_StatusTypeDef platform_uart_backend_receive(
  Platform_UART_Instance uart,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  UART_HandleTypeDef *huart = platform_uart_to_hal(uart);
  return huart == NULL
           ? Platform_INVALID_ARGUMENT
           : platform_from_hal_status(
               HAL_UART_Receive(huart, data, size, timeout)
             );
}

Platform_StatusTypeDef platform_uart_backend_transmit(
  Platform_UART_Instance uart,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  UART_HandleTypeDef *huart = platform_uart_to_hal(uart);
  return huart == NULL
           ? Platform_INVALID_ARGUMENT
           : platform_from_hal_status(
               HAL_UART_Transmit(huart, (uint8_t *)data, size, timeout)
             );
}

Platform_StatusTypeDef platform_uart_backend_receive_it(
  Platform_UART_Instance uart,
  uint8_t *data,
  uint16_t size
) {
  UART_HandleTypeDef *huart = platform_uart_to_hal(uart);
  return huart == NULL
           ? Platform_INVALID_ARGUMENT
           : platform_from_hal_status(
               HAL_UART_Receive_IT(huart, data, size)
             );
}

Platform_StatusTypeDef platform_uart_backend_transmit_it(
  Platform_UART_Instance uart,
  const uint8_t *data,
  uint16_t size
) {
  UART_HandleTypeDef *huart = platform_uart_to_hal(uart);
  return huart == NULL
           ? Platform_INVALID_ARGUMENT
           : platform_from_hal_status(
               HAL_UART_Transmit_IT(huart, (uint8_t *)data, size)
             );
}

Platform_StatusTypeDef platform_uart_backend_receive_dma(
  Platform_UART_Instance uart,
  uint8_t *data,
  uint16_t size
) {
  UART_HandleTypeDef *huart = platform_uart_to_hal(uart);
  return huart == NULL
           ? Platform_INVALID_ARGUMENT
           : platform_from_hal_status(
               HAL_UART_Receive_DMA(huart, data, size)
             );
}

Platform_StatusTypeDef platform_uart_backend_transmit_dma(
  Platform_UART_Instance uart,
  const uint8_t *data,
  uint16_t size
) {
  UART_HandleTypeDef *huart = platform_uart_to_hal(uart);
  return huart == NULL
           ? Platform_INVALID_ARGUMENT
           : platform_from_hal_status(
               HAL_UART_Transmit_DMA(huart, (uint8_t *)data, size)
             );
}

Platform_StatusTypeDef platform_uart_backend_abort_receive(
  Platform_UART_Instance uart
) {
  UART_HandleTypeDef *huart = platform_uart_to_hal(uart);
  if (huart == NULL) {
    return Platform_INVALID_ARGUMENT;
  }

  HAL_StatusTypeDef status = HAL_UART_AbortReceive(huart);
  if (status == HAL_OK) {
    platform_uart_clear_rx_flags(huart);
    huart->ErrorCode = HAL_UART_ERROR_NONE;
  }
  return platform_from_hal_status(status);
}

Platform_StatusTypeDef platform_uart_backend_abort_transmit(
  Platform_UART_Instance uart
) {
  UART_HandleTypeDef *huart = platform_uart_to_hal(uart);
  if (huart == NULL) {
    return Platform_INVALID_ARGUMENT;
  }

  HAL_StatusTypeDef status = HAL_UART_AbortTransmit(huart);
  if (status == HAL_OK) {
    huart->ErrorCode = HAL_UART_ERROR_NONE;
  }
  return platform_from_hal_status(status);
}

Platform_StatusTypeDef platform_uart_backend_abort(
  Platform_UART_Instance uart
) {
  UART_HandleTypeDef *huart = platform_uart_to_hal(uart);
  if (huart == NULL) {
    return Platform_INVALID_ARGUMENT;
  }

  HAL_StatusTypeDef status = HAL_UART_Abort(huart);
  if (status == HAL_OK) {
    platform_uart_clear_rx_flags(huart);
    huart->ErrorCode = HAL_UART_ERROR_NONE;
  }
  return platform_from_hal_status(status);
}

uint32_t platform_uart_backend_get_error(Platform_UART_Instance uart) {
  UART_HandleTypeDef *huart = platform_uart_to_hal(uart);
  return huart == NULL
           ? PLATFORM_UART_ERROR_UNKNOWN
           : platform_uart_map_error(huart->ErrorCode);
}

static void platform_uart_dispatch_hal_event(
  UART_HandleTypeDef *huart,
  Platform_UART_EventType type
) {
  Platform_UART_Instance uart;
  if (platform_hal_to_uart(huart, &uart)) {
    platform_uart_dispatch_event(uart, type);
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
  platform_uart_dispatch_hal_event(huart, PLATFORM_UART_EVENT_TX_CPLT);
}

void HAL_UART_TxHalfCpltCallback(UART_HandleTypeDef *huart) {
  platform_uart_dispatch_hal_event(huart, PLATFORM_UART_EVENT_TX_HALF);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  platform_uart_dispatch_hal_event(huart, PLATFORM_UART_EVENT_RX_CPLT);
}

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart) {
  platform_uart_dispatch_hal_event(huart, PLATFORM_UART_EVENT_RX_HALF);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  platform_uart_dispatch_hal_event(huart, PLATFORM_UART_EVENT_ERROR);
}

void HAL_UART_AbortCpltCallback(UART_HandleTypeDef *huart) {
  platform_uart_dispatch_hal_event(huart, PLATFORM_UART_EVENT_ABORT_CPLT);
}

void HAL_UART_AbortTransmitCpltCallback(UART_HandleTypeDef *huart) {
  platform_uart_dispatch_hal_event(
    huart,
    PLATFORM_UART_EVENT_ABORT_TX_CPLT
  );
}

void HAL_UART_AbortReceiveCpltCallback(UART_HandleTypeDef *huart) {
  platform_uart_dispatch_hal_event(
    huart,
    PLATFORM_UART_EVENT_ABORT_RX_CPLT
  );
}
