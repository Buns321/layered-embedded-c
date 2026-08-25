//
// Created by Buns_ on 2026/8/26.
//

#ifndef LAYERED_EMBEDDED_C_USART_PINGPONG_H
#define LAYERED_EMBEDDED_C_USART_PINGPONG_H

#include "../../DeviceDrivers/Base/Inc/usart_base.h"
#include "../../Platform/Inc/platform_usart.h"

typedef struct {
  usart_base_t usart_base;
  Platform_UART_Instance uart;
  uint8_t *buf;  uint16_t buf_size;
  uint8_t *data; uint16_t data_size;
} usart_pingpong_t;

void Dev_USART_PingPong_Init(
  usart_pingpong_t *this, const char *name,
  Platform_UART_Instance  uart,
  uint8_t          *buf,  uint16_t   buf_size,
  uint8_t          *data, uint16_t   data_size);

#endif //LAYERED_EMBEDDED_C_USART_PINGPONG_H
