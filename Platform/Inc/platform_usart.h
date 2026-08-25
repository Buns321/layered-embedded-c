//
// Created by Buns_ on 2026/8/24.
//

#ifndef LAYERED_EMBEDDED_C_PLATFORM_USART_H
#define LAYERED_EMBEDDED_C_PLATFORM_USART_H

#include "platform_def.h"
#include <stdint.h>

typedef enum {
  PLATFORM_UART_1 = 0U,
  PLATFORM_UART_2,
  PLATFORM_UART_3,
} Platform_UART_Instance;

typedef void (*Platform_UART_Callback)(Platform_UART_Instance uart, void *context);

typedef struct {
  Platform_UART_Callback tx_cplt_cb;
  Platform_UART_Callback tx_halfcplt_cb;
  Platform_UART_Callback rx_cplt_cb;
  Platform_UART_Callback rx_halfcplt_cb;
  Platform_UART_Callback err_cb;
  Platform_UART_Callback abort_cplt_cb;
  Platform_UART_Callback abort_txcplt_cb;
  Platform_UART_Callback abort_rxcplt_cb;
  void *context;
} platform_uart_callbacks_t;

Platform_StatusTypeDef Platform_UART_Receive(Platform_UART_Instance uart, uint8_t *pdata, uint16_t size, uint32_t timeout);
Platform_StatusTypeDef Platform_UART_Transmit(Platform_UART_Instance uart, uint8_t *pdata, uint16_t size, uint32_t timeout);
Platform_StatusTypeDef Platform_UART_Receive_IT(Platform_UART_Instance uart, uint8_t *pdata, uint16_t size);
Platform_StatusTypeDef Platform_UART_ClearBuffer(Platform_UART_Instance uart, uint8_t *pdata);
void Platform_UART_RegisterCallbacks(Platform_UART_Instance uart, const platform_uart_callbacks_t *cb);

#endif //LAYERED_EMBEDDED_C_PLATFORM_USART_H
