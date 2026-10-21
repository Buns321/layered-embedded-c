//
// Created by Buns_ on 2026/8/24.
//

#include "platform_usart.h"
#include "platform_hal_internal.h"
#include "stm32f4xx_hal.h"
#include "usart.h"
#include <assert.h>
#include <stdbool.h>

static platform_uart_callbacks_t uart_cb[PLATFORM_UART_INSTANCE_COUNT] = {{0}};

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
    default: assert(0); return NULL;
  }
}

static bool platform_hal_to_uart(
  UART_HandleTypeDef *huart,
  Platform_UART_Instance *uart
) {
  assert(huart != NULL && uart != NULL);

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

static void platform_uart_validate(
  Platform_UART_Instance uart,
  const void *data,
  uint16_t size
) {
  assert(
    data != NULL &&
    size != 0U &&
    (uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT
  );
  (void)data;
  (void)size;
  (void)uart;
}

Platform_StatusTypeDef Platform_UART_Receive(
  Platform_UART_Instance uart,
  uint8_t *pdata,
  uint16_t size,
  uint32_t timeout
) {
  platform_uart_validate(uart, pdata, size);
  UART_HandleTypeDef *huart = platform_uart_to_hal(uart);
  assert(huart != NULL);

  return platform_from_hal_status(
    HAL_UART_Receive(huart, pdata, size, timeout)
  );
}

Platform_StatusTypeDef Platform_UART_Transmit(
  Platform_UART_Instance uart,
  const uint8_t *pdata,
  uint16_t size,
  uint32_t timeout
) {
  platform_uart_validate(uart, pdata, size);
  UART_HandleTypeDef *huart = platform_uart_to_hal(uart);
  assert(huart != NULL);

  return platform_from_hal_status(
    HAL_UART_Transmit(huart, pdata, size, timeout)
  );
}

Platform_StatusTypeDef Platform_UART_Receive_IT(
  Platform_UART_Instance uart,
  uint8_t *pdata,
  uint16_t size
) {
  platform_uart_validate(uart, pdata, size);
  UART_HandleTypeDef *huart = platform_uart_to_hal(uart);
  assert(huart != NULL);

  return platform_from_hal_status(
    HAL_UART_Receive_IT(huart, pdata, size)
  );
}

Platform_StatusTypeDef Platform_UART_Transmit_IT(
  Platform_UART_Instance uart,
  const uint8_t *pdata,
  uint16_t size
) {
  platform_uart_validate(uart, pdata, size);
  UART_HandleTypeDef *huart = platform_uart_to_hal(uart);
  assert(huart != NULL);

  return platform_from_hal_status(
    HAL_UART_Transmit_IT(huart, pdata, size)
  );
}

Platform_StatusTypeDef Platform_UART_Receive_DMA(
  Platform_UART_Instance uart,
  uint8_t *pdata,
  uint16_t size
) {
  platform_uart_validate(uart, pdata, size);
  UART_HandleTypeDef *huart = platform_uart_to_hal(uart);
  assert(huart != NULL);

  return platform_from_hal_status(
    HAL_UART_Receive_DMA(huart, pdata, size)
  );
}

Platform_StatusTypeDef Platform_UART_Transmit_DMA(
  Platform_UART_Instance uart,
  const uint8_t *pdata,
  uint16_t size
) {
  platform_uart_validate(uart, pdata, size);
  UART_HandleTypeDef *huart = platform_uart_to_hal(uart);
  assert(huart != NULL);

  return platform_from_hal_status(
    HAL_UART_Transmit_DMA(huart, pdata, size)
  );
}

Platform_StatusTypeDef Platform_UART_AbortReceive(
  Platform_UART_Instance uart
) {
  assert((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT);
  UART_HandleTypeDef *huart = platform_uart_to_hal(uart);
  assert(huart != NULL);

  HAL_StatusTypeDef status = HAL_UART_AbortReceive(huart);
  if (status == HAL_OK) {
    platform_uart_clear_rx_flags(huart);
    huart->ErrorCode = HAL_UART_ERROR_NONE;
  }

  return platform_from_hal_status(status);
}

Platform_StatusTypeDef Platform_UART_AbortTransmit(
  Platform_UART_Instance uart
) {
  assert((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT);
  UART_HandleTypeDef *huart = platform_uart_to_hal(uart);
  assert(huart != NULL);

  HAL_StatusTypeDef status = HAL_UART_AbortTransmit(huart);
  if (status == HAL_OK) {
    huart->ErrorCode = HAL_UART_ERROR_NONE;
  }

  return platform_from_hal_status(status);
}

Platform_StatusTypeDef Platform_UART_Abort(
  Platform_UART_Instance uart
) {
  assert((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT);
  UART_HandleTypeDef *huart = platform_uart_to_hal(uart);
  assert(huart != NULL);

  HAL_StatusTypeDef status = HAL_UART_Abort(huart);
  if (status == HAL_OK) {
    platform_uart_clear_rx_flags(huart);
    huart->ErrorCode = HAL_UART_ERROR_NONE;
  }

  return platform_from_hal_status(status);
}

Platform_StatusTypeDef Platform_UART_ClearBuffer(
  Platform_UART_Instance uart
) {
  return Platform_UART_AbortReceive(uart);
}

uint32_t Platform_UART_GetLastError(
  Platform_UART_Instance uart
) {
  assert((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT);
  UART_HandleTypeDef *huart = platform_uart_to_hal(uart);
  assert(huart != NULL);
  return platform_uart_map_error(huart->ErrorCode);
}

void Platform_UART_RegisterCallbacks(
  Platform_UART_Instance uart,
  const platform_uart_callbacks_t *cb
) {
  assert(
    (uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT &&
    cb != NULL &&
    platform_uart_to_hal(uart) != NULL
  );

  uart_cb[uart] = *cb;
}

typedef enum {
  PLATFORM_UART_EVENT_TX_CPLT = 0,
  PLATFORM_UART_EVENT_TX_HALF,
  PLATFORM_UART_EVENT_RX_CPLT,
  PLATFORM_UART_EVENT_RX_HALF,
  PLATFORM_UART_EVENT_ERROR,
  PLATFORM_UART_EVENT_ABORT_CPLT,
  PLATFORM_UART_EVENT_ABORT_TX_CPLT,
  PLATFORM_UART_EVENT_ABORT_RX_CPLT,
} platform_uart_event_t;

static void platform_uart_dispatch(
  UART_HandleTypeDef *huart,
  platform_uart_event_t event
) {
  assert(huart != NULL);

  Platform_UART_Instance uart;
  if (!platform_hal_to_uart(huart, &uart)) {
    return;
  }

  Platform_UART_Callback callback = NULL;
  switch (event) {
    case PLATFORM_UART_EVENT_TX_CPLT:
      callback = uart_cb[uart].tx_cplt_cb;
      break;
    case PLATFORM_UART_EVENT_TX_HALF:
      callback = uart_cb[uart].tx_halfcplt_cb;
      break;
    case PLATFORM_UART_EVENT_RX_CPLT:
      callback = uart_cb[uart].rx_cplt_cb;
      break;
    case PLATFORM_UART_EVENT_RX_HALF:
      callback = uart_cb[uart].rx_halfcplt_cb;
      break;
    case PLATFORM_UART_EVENT_ERROR:
      callback = uart_cb[uart].err_cb;
      break;
    case PLATFORM_UART_EVENT_ABORT_CPLT:
      callback = uart_cb[uart].abort_cplt_cb;
      break;
    case PLATFORM_UART_EVENT_ABORT_TX_CPLT:
      callback = uart_cb[uart].abort_txcplt_cb;
      break;
    case PLATFORM_UART_EVENT_ABORT_RX_CPLT:
      callback = uart_cb[uart].abort_rxcplt_cb;
      break;
    default:
      assert(0);
      break;
  }

  if (callback != NULL) {
    callback(uart, uart_cb[uart].context);
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
  platform_uart_dispatch(huart, PLATFORM_UART_EVENT_TX_CPLT);
}

void HAL_UART_TxHalfCpltCallback(UART_HandleTypeDef *huart) {
  platform_uart_dispatch(huart, PLATFORM_UART_EVENT_TX_HALF);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  platform_uart_dispatch(huart, PLATFORM_UART_EVENT_RX_CPLT);
}

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart) {
  platform_uart_dispatch(huart, PLATFORM_UART_EVENT_RX_HALF);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  platform_uart_dispatch(huart, PLATFORM_UART_EVENT_ERROR);
}

void HAL_UART_AbortCpltCallback(UART_HandleTypeDef *huart) {
  platform_uart_dispatch(huart, PLATFORM_UART_EVENT_ABORT_CPLT);
}

void HAL_UART_AbortTransmitCpltCallback(UART_HandleTypeDef *huart) {
  platform_uart_dispatch(huart, PLATFORM_UART_EVENT_ABORT_TX_CPLT);
}

void HAL_UART_AbortReceiveCpltCallback(UART_HandleTypeDef *huart) {
  platform_uart_dispatch(huart, PLATFORM_UART_EVENT_ABORT_RX_CPLT);
}
