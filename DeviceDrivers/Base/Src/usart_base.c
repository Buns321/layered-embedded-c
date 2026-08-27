//
// Created by Buns_ on 2026/8/26.
//

#include "../Inc/usart_base.h"
#include <assert.h>

#include "../../../Platform/Inc/platform_usart.h"

void Dev_USART_Base_Init(usart_base_t *this, const char *name, const usart_ops_t *ops) {
  assert(this && name && ops);
  this->ops              = ops;
  this->name             = name;
  this->is_usart_enabled = DEV_USART_ON;
}

Dev_USART_State Dev_USART_Base_ReadStatus(const usart_base_t *this) {
  assert((this));
  return this->is_usart_enabled;
}

Dev_USART_State Dev_USART_Base_Receive(usart_base_t *this, uint8_t *buf, uint16_t size) {
  assert(this && buf);
  return this->ops->receive(this, buf,size);
}

Dev_USART_State Dev_USART_Base_Transmit(usart_base_t *this, const uint8_t *data, uint16_t size) {
  assert(this && data);
  return this->ops->transmit(this, data, size);
}

