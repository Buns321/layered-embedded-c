/**
 * @file platform_uart_backend.h
 * @brief STM32 系列公共 UART 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_UART_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_UART_BACKEND_H

#include "platform_def.h"
#include "platform_uart.h"
#include <stdbool.h>
#include <stdint.h>

bool platform_uart_backend_is_valid(platform_uart_instance_t uart);

platform_status_t platform_uart_backend_receive(
  platform_uart_instance_t uart,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

platform_status_t platform_uart_backend_transmit(
  platform_uart_instance_t uart,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

platform_status_t platform_uart_backend_receive_it(
  platform_uart_instance_t uart,
  uint8_t *data,
  uint16_t size
);

platform_status_t platform_uart_backend_transmit_it(
  platform_uart_instance_t uart,
  const uint8_t *data,
  uint16_t size
);

platform_status_t platform_uart_backend_receive_dma(
  platform_uart_instance_t uart,
  uint8_t *data,
  uint16_t size
);

platform_status_t platform_uart_backend_transmit_dma(
  platform_uart_instance_t uart,
  const uint8_t *data,
  uint16_t size
);

platform_status_t platform_uart_backend_abort_receive(
  platform_uart_instance_t uart
);

platform_status_t platform_uart_backend_abort_transmit(
  platform_uart_instance_t uart
);

platform_status_t platform_uart_backend_abort(
  platform_uart_instance_t uart
);

uint32_t platform_uart_backend_get_error(platform_uart_instance_t uart);

void platform_uart_dispatch_event(
  platform_uart_instance_t uart,
  platform_uart_event_type_t type
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_UART_BACKEND_H
