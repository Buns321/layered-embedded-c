//
// Created by Buns_ on 2026/8/24.
//

#include "../../Inc/platform_usart.h"
#include "stm32f1xx_hal.h"
#include "usart.h"
#include <assert.h>

static UART_HandleTypeDef *platform_to_hal_uart(Platform_UART_Instance uart) {
  switch (uart) {
    // case PLATFORM_UART_1: return &huart1
    case PLATFORM_UART_2: return &huart2;
    // case PLATFORM_UART_3: return &huart3
    default:              assert(0); return NULL;
  }
}

static Platform_StatusTypeDef hal_to_platform_statustypedef(HAL_StatusTypeDef statustypedef) {
  switch (statustypedef) {
    case HAL_OK:      return Platform_OK;
    case HAL_ERROR:   return Platform_ERROR;
    case HAL_BUSY:    return Platform_BUSY;
    case HAL_TIMEOUT: return Platform_TIMEOUT;
    default:          assert(0); return Platform_ERROR;
  }
}

Platform_StatusTypeDef Platform_UART_Receive(Platform_UART_Instance uart, uint8_t *pdata, uint16_t size, uint32_t timeout) {
  UART_HandleTypeDef *huart = platform_to_hal_uart(uart);
  assert(huart != NULL);
  return hal_to_platform_statustypedef(HAL_UART_Receive(huart, pdata, size, timeout));
}

Platform_StatusTypeDef Platform_UART_Transmit(Platform_UART_Instance uart, uint8_t *pdata, uint16_t size, uint32_t timeout) {
  UART_HandleTypeDef *huart = platform_to_hal_uart(uart);
  assert(huart != NULL);
  return hal_to_platform_statustypedef(HAL_UART_Transmit(huart, pdata, size, timeout));
}
