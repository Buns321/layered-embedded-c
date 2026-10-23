/**
 * @file platform_uart_backend.h
 * @brief Private STM32-family backend for the common UART implementation.
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_UART_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_UART_BACKEND_H

#include "platform_def.h"
#include "platform_usart.h"
#include <stdbool.h>
#include <stdint.h>

bool platform_uart_backend_is_valid(Platform_UART_Instance uart);

Platform_StatusTypeDef platform_uart_backend_receive(
  Platform_UART_Instance uart,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

Platform_StatusTypeDef platform_uart_backend_transmit(
  Platform_UART_Instance uart,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

Platform_StatusTypeDef platform_uart_backend_receive_it(
  Platform_UART_Instance uart,
  uint8_t *data,
  uint16_t size
);

Platform_StatusTypeDef platform_uart_backend_transmit_it(
  Platform_UART_Instance uart,
  const uint8_t *data,
  uint16_t size
);

Platform_StatusTypeDef platform_uart_backend_receive_dma(
  Platform_UART_Instance uart,
  uint8_t *data,
  uint16_t size
);

Platform_StatusTypeDef platform_uart_backend_transmit_dma(
  Platform_UART_Instance uart,
  const uint8_t *data,
  uint16_t size
);

Platform_StatusTypeDef platform_uart_backend_abort_receive(
  Platform_UART_Instance uart
);

Platform_StatusTypeDef platform_uart_backend_abort_transmit(
  Platform_UART_Instance uart
);

Platform_StatusTypeDef platform_uart_backend_abort(
  Platform_UART_Instance uart
);

uint32_t platform_uart_backend_get_error(Platform_UART_Instance uart);

void platform_uart_dispatch_event(
  Platform_UART_Instance uart,
  Platform_UART_EventType type
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_UART_BACKEND_H
