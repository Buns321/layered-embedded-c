//
// Created by Buns_ on 2026/8/26.
//

#include "../Inc/usart_pingpong.h"
#include "../../Platform/Inc/platform_usart.h"
#include <assert.h>

static void Dev_USART_PingPong_Receive(usart_base_t *this, uint8_t *buf, uint16_t size) {
  assert(this);
  usart_pingpong_t *self = (usart_pingpong_t *)this;
  Platform_UART_Receive_DMA(self->uart, buf, size);
}

static void Dev_USART_PingPong_Transmit(usart_base_t *this, uint8_t *data, uint16_t size) {
  assert(this);
  usart_pingpong_t *self = (usart_pingpong_t *)this;
  Platform_UART_Transmit_DMA(self->uart, data, size);
}

static const usart_ops_t usart_ops_pingpong = {
  .receive  = Dev_USART_PingPong_Receive,
  .transmit = Dev_USART_PingPong_Transmit,
};

void Dev_USART_PingPong_Init(
  usart_pingpong_t *this, const char *name,
  Platform_UART_Instance  uart,
  uint8_t          *buf,  uint16_t   buf_size,
  uint8_t          *data, uint16_t   data_size) {

  assert(this && name && buf && data);

  Dev_USART_Base_Init(&this->usart_base, name, &usart_ops_pingpong);

  this->uart      = uart;
  this->buf_size  = buf_size;
  this->data_size = data_size;
}
