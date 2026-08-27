//
// Created by Buns_ on 2026/8/26.
//

#include "../Inc/usart_pingpong.h"
#include "../../Platform/Inc/platform_usart.h"
#include <assert.h>
#include <string.h>

static void pingpong_rx_halfcplt_cb(Platform_UART_Instance uart, void *context) {
  usart_pingpong_t *self = (usart_pingpong_t *)context;
  uint16_t n = self->buf_size / 2;
  memcpy(self->data, self->buf, n);
  self->is_data_ready = DEV_USART_PINGPONG_DATA_READY;
}

static void pingpong_rx_cplt_cb(Platform_UART_Instance uart, void *context) {
  usart_pingpong_t *self = (usart_pingpong_t *)context;
  uint16_t n = self->buf_size / 2;
  memcpy(self->data, self->buf + n, n);
  self->is_data_ready = DEV_USART_PINGPONG_DATA_READY;
}

void Dev_USART_PingPong_Init(
  usart_pingpong_t *this, const char *name,
  Platform_UART_Instance  uart,
  pingpong_handle handle,
  uint8_t          *buf,  uint16_t   buf_size,
  uint8_t          *data, uint16_t   data_size) {
  assert(this && name && buf && data);
  this->uart   = uart;  this->handle = handle;
  this->is_data_ready = DEV_USART_PINGPONG_DATA_NOTREADY;
  this->buf    = buf;   this->buf_size      = buf_size;
  this->data   = data;  this->data_size     = data_size;
  platform_uart_callbacks_t cb = {
    .rx_halfcplt_cb = pingpong_rx_halfcplt_cb,
    .rx_cplt_cb = pingpong_rx_cplt_cb,
    .context = this,
  };
  Platform_UART_RegisterCallbacks(uart, &cb);
  Platform_UART_Receive_DMA(this->uart, buf, buf_size);
}

void Dev_USART_PingPong_Process(usart_pingpong_t *self, void *context) {
  if (self->is_data_ready == DEV_USART_PINGPONG_DATA_READY) {
    self->is_data_ready = DEV_USART_PINGPONG_DATA_NOTREADY;
    uint16_t n = self->buf_size / 2;
    if (self->handle) self->handle(self->data, n, context);
  }
}

