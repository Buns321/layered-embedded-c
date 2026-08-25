//
// Created by Buns_ on 2026/8/24.
//

#include "../../Inc/platform_usart.h"
#include "stm32f1xx_hal.h"
#include "usart.h"
#include <assert.h>

static platform_uart_callbacks_t uart_cb[3] = {{0}}; // 置零所有结构体成员

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

static Platform_UART_Instance hal_to_platform_huart(UART_HandleTypeDef *huart) {
  // if (huart == &huart1) return PLATFORM_UART_1;
  if (huart == &huart2) return PLATFORM_UART_2;
  // if (huart == &huart3) return PLATFORM_UART_3;
  assert(0);        return 0;
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

Platform_StatusTypeDef Platform_UART_Receive_IT(Platform_UART_Instance uart, uint8_t *pdata, uint16_t size) {
  UART_HandleTypeDef *huart = platform_to_hal_uart(uart);
  assert(huart != NULL);
  return hal_to_platform_statustypedef(HAL_UART_Receive_IT(huart, pdata, size));
}

Platform_StatusTypeDef Platform_UART_Transmit_IT(Platform_UART_Instance uart, uint8_t *pdata, uint16_t size) {
  UART_HandleTypeDef *huart = platform_to_hal_uart(uart);
  assert(huart != NULL);
  return hal_to_platform_statustypedef(HAL_UART_Transmit_IT(huart, pdata, size));
}

Platform_StatusTypeDef Platform_UART_Receive_DMA(Platform_UART_Instance uart, uint8_t *pdata, uint16_t size) {
  UART_HandleTypeDef *huart = platform_to_hal_uart(uart);
  assert(huart != NULL);
  return hal_to_platform_statustypedef(HAL_UART_Receive_DMA(huart, pdata, size));
}

Platform_StatusTypeDef Platform_UART_Transmit_DMA(Platform_UART_Instance uart, uint8_t *pdata, uint16_t size) {
  UART_HandleTypeDef *huart = platform_to_hal_uart(uart);
  assert(huart != NULL);
  return hal_to_platform_statustypedef(HAL_UART_Transmit_DMA(huart, pdata, size));
}

Platform_StatusTypeDef Platform_UART_ClearBuffer(Platform_UART_Instance uart, uint8_t *pdata) {
  UART_HandleTypeDef *huart = platform_to_hal_uart(uart);
  assert(huart != NULL);
  return hal_to_platform_statustypedef(HAL_UART_Receive(huart, pdata, 1, 0));
}

void Platform_UART_RegisterCallbacks(Platform_UART_Instance uart, const platform_uart_callbacks_t *cb) {
  assert(cb);
  uart_cb[uart] = *cb;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
  assert(huart);
  Platform_UART_Instance uart = hal_to_platform_huart(huart);
  if (uart_cb[uart].tx_cplt_cb) uart_cb[uart].tx_cplt_cb(uart, uart_cb[uart].context);
}

void HAL_UART_TxHalfCpltCallback(UART_HandleTypeDef *huart) {
  assert(huart);
  Platform_UART_Instance uart = hal_to_platform_huart(huart);
  if (uart_cb[uart].tx_halfcplt_cb) uart_cb[uart].tx_halfcplt_cb(uart, uart_cb[uart].context);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  assert(huart);
  Platform_UART_Instance uart = hal_to_platform_huart(huart);
  if (uart_cb[uart].rx_cplt_cb) uart_cb[uart].rx_cplt_cb(uart, uart_cb[uart].context);
}

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef *huart) {
  assert(huart);
  Platform_UART_Instance uart = hal_to_platform_huart(huart);
  if (uart_cb[uart].rx_halfcplt_cb) uart_cb[uart].rx_halfcplt_cb(uart, uart_cb[uart].context);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  assert(huart);
  Platform_UART_Instance uart = hal_to_platform_huart(huart);
  if (uart_cb[uart].err_cb) uart_cb[uart].err_cb(uart, uart_cb[uart].context);
}

void HAL_UART_AbortCpltCallback(UART_HandleTypeDef *huart) {
  assert(huart);
  Platform_UART_Instance uart = hal_to_platform_huart(huart);
  if (uart_cb[uart].abort_cplt_cb) uart_cb[uart].abort_cplt_cb(uart, uart_cb[uart].context);
}
void HAL_UART_AbortTransmitCpltCallback(UART_HandleTypeDef *huart) {
  assert(huart);
  Platform_UART_Instance uart = hal_to_platform_huart(huart);
  if (uart_cb[uart].abort_txcplt_cb) uart_cb[uart].abort_txcplt_cb(uart, uart_cb[uart].context);
}

void HAL_UART_AbortReceiveCpltCallback(UART_HandleTypeDef *huart) {
  assert(huart);
  Platform_UART_Instance uart = hal_to_platform_huart(huart);
  if (uart_cb[uart].abort_rxcplt_cb) uart_cb[uart].abort_rxcplt_cb(uart, uart_cb[uart].context);
}
