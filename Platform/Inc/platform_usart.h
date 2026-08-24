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

Platform_StatusTypeDef Platform_UART_Receive(Platform_UART_Instance uart, uint8_t *pdata, uint16_t size, uint32_t timeout);
Platform_StatusTypeDef Platform_UART_Transmit(Platform_UART_Instance uart, uint8_t *pdata, uint16_t size, uint32_t timeout);

#endif //LAYERED_EMBEDDED_C_PLATFORM_USART_H
