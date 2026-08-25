//
// Created by Buns_ on 2026/8/26.
//

#ifndef LAYERED_EMBEDDED_C_USART_BASE_H
#define LAYERED_EMBEDDED_C_USART_BASE_H

#include <stdint.h>

#include "../../../Platform/Inc/platform_usart.h"

typedef enum {
  DEV_USART_OFF = 0U,
  DEV_USART_ON,
} Dev_USART_State;

typedef struct usart_base usart_base_t;

typedef struct usart_ops {
  void (*receive) (usart_base_t *this, uint8_t *buf, uint16_t size);
  void (*transmit) (usart_base_t *this, uint8_t *data, uint16_t size);
} usart_ops_t;

typedef struct usart_base {
  const usart_ops_t *ops;
  const char *name;
  Dev_USART_State is_usart_enabled;
} usart_base_t;

void Dev_USART_Base_Init(usart_base_t *this, const char *name, const usart_ops_t *ops);
Dev_USART_State Dev_USART_Base_ReadStatus(const usart_base_t *this);
void Dev_USART_Base_Receive(usart_base_t *this, uint8_t *buf, uint16_t size);
void Dev_USART_Base_Transmit(usart_base_t *this, const uint8_t *data, uint16_t size);

#endif //LAYERED_EMBEDDED_C_USART_BASE_H
