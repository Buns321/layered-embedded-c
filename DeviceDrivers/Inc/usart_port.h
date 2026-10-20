//
// Created by Buns_ on 2026/10/20.
//

#ifndef LAYERED_EMBEDDED_C_USART_PORT_H
#define LAYERED_EMBEDDED_C_USART_PORT_H

#include "platform_usart.h"
#include "usart_base.h"

typedef struct usart_port {
  usart_base_t usart_base;
  Platform_UART_Instance instance;
} usart_port_t;

void Dev_USART_Port_Init(
  usart_port_t *this,
  const char *name,
  Platform_UART_Instance instance
);

#endif //LAYERED_EMBEDDED_C_USART_PORT_H
