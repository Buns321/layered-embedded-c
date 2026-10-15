//
// Created by Buns_ on 2026/8/26.
//

#include "../Inc/usart_base.h"
#include <assert.h>

void Dev_USART_Base_Init(usart_base_t *this, const char *name, const usart_ops_t *ops) {
  assert(this && name && ops);
  this->ops              = ops;
  this->name             = name;
  this->is_usart_enabled = DEV_USART_ON;
  this->callbacks        = (dev_usart_callbacks_t){0};
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

Dev_USART_State Dev_USART_Receive_IT(usart_base_t *this, uint8_t *pdata, uint16_t size) {
  assert(this && pdata);
  return this->ops->receive_it(this, pdata, size);
}

Dev_USART_State Dev_USART_Transmit_IT(usart_base_t *this, uint8_t *pdata, uint16_t size){
  assert(this && pdata);
  return this->ops->transmit_it(this, pdata, size);
}

Dev_USART_State Dev_USART_Receive_DMA(usart_base_t *this, uint8_t *pdata, uint16_t size) {
  assert(this && pdata);
  return this->ops->receive_dma(this, pdata, size);
}

Dev_USART_State Dev_USART_Transmit_DMA(usart_base_t *this, uint8_t *pdata, uint16_t size) {
  assert(this && pdata);
  return this->ops->transmit_dma(this, pdata, size);
}

Dev_USART_State Dev_USART_ClearBuffer(usart_base_t *this, uint8_t *pdata) {
  assert(this && pdata);
  return this->ops->clearbuffer(this, pdata);
}

void Dev_USART_RegisterCallbacks(usart_base_t *this, const dev_usart_callbacks_t *cb) {
  assert(this && cb);
  this->callbacks = *cb;
}

void Dev_USART_Base_OnTxCplt(usart_base_t *this) {
  assert(this);
  if (this->callbacks.tx_cplt_cb) {
    this->callbacks.tx_cplt_cb(this, this->callbacks.context);
  }
}

void Dev_USART_Base_OnTxHalfCplt(usart_base_t *this){
  assert(this);
  if (this->callbacks.tx_halfcplt_cb) {
    this->callbacks.tx_halfcplt_cb(this, this->callbacks.context);
  }
}

void Dev_USART_Base_OnRxCplt(usart_base_t *this) {
  assert(this);
  if (this->callbacks.rx_cplt_cb) {
    this->callbacks.rx_cplt_cb(this, this->callbacks.context);
  }
}

void Dev_USART_Base_OnRxHalfCplt(usart_base_t *this){
  assert(this);
  if (this->callbacks.rx_halfcplt_cb) {
    this->callbacks.rx_halfcplt_cb(this, this->callbacks.context);
  }
}

void Dev_USART_Base_OnError(usart_base_t *this){
  assert(this);
  if (this->callbacks.err_cb) {
    this->callbacks.err_cb(this, this->callbacks.context);
  }
}

void Dev_USART_Base_OnAbortCplt(usart_base_t *this){
  assert(this);
  if (this->callbacks.abort_cplt_cb) {
    this->callbacks.abort_cplt_cb(this, this->callbacks.context);
  }
}

void Dev_USART_Base_OnAbortTxCplt(usart_base_t *this){
  assert(this);
  if (this->callbacks.abort_txcplt_cb) {
    this->callbacks.abort_txcplt_cb(this, this->callbacks.context);
  }
}

void Dev_USART_Base_OnAbortRxCplt(usart_base_t *this){
  assert(this);
  if (this->callbacks.abort_rxcplt_cb) {
    this->callbacks.abort_rxcplt_cb(this, this->callbacks.context);
  }
}
