//
// Created by Buns_ on 2026/8/26.
//

#ifndef LAYERED_EMBEDDED_C_USART_PINGPONG_H
#define LAYERED_EMBEDDED_C_USART_PINGPONG_H

#include "../../Platform/Inc/platform_usart.h"

typedef enum {
  DEV_USART_PINGPONG_DATA_NOTREADY = 0U,
  DEV_USART_PINGPONG_DATA_READY,
} Dev_USART_PingPong_Data_State;

typedef void (*pingpong_handle)(uint8_t *data, uint16_t data_size, void *context);

typedef struct {
  Platform_UART_Instance uart;
  volatile Dev_USART_PingPong_Data_State is_data_ready;
  pingpong_handle handle;
  uint8_t *buf;  uint16_t buf_size;
  uint8_t *data; uint16_t data_size;
} usart_pingpong_t;

void Dev_USART_PingPong_Init(
  usart_pingpong_t *this, const char *name,
  Platform_UART_Instance  uart,
  pingpong_handle handle,
  uint8_t          *buf,  uint16_t   buf_size,
  uint8_t          *data, uint16_t   data_size);
void Dev_USART_PingPong_Process(usart_pingpong_t *self, void *context);

#endif //LAYERED_EMBEDDED_C_USART_PINGPONG_H
