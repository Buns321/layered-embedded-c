//
// 由 Buns_ 创建于 2026/10/20。
//

#include "usart_port.h"
#include <assert.h>

static usart_port_t *Dev_USART_Port_FromBase(usart_base_t *this) {
  assert(this);
  return (usart_port_t *)this;
}

static Dev_USART_Status Dev_USART_Port_MapStatus(
  Platform_StatusTypeDef status
) {
  switch (status) {
    case Platform_OK:               return DEV_USART_OK;
    case Platform_BUSY:             return DEV_USART_BUSY;
    case Platform_TIMEOUT:          return DEV_USART_TIMEOUT;
    case Platform_ERROR:            return DEV_USART_ERROR;
    case Platform_INVALID_ARGUMENT: return DEV_USART_ERROR;
    case Platform_UNSUPPORTED:      return DEV_USART_ERROR;
    default:                        assert(0); return DEV_USART_ERROR;
  }
}

static Dev_USART_Status Dev_USART_Port_Receive(
  usart_base_t *this,
  uint8_t *buf,
  uint16_t size,
  uint32_t timeout
) {
  usart_port_t *self = Dev_USART_Port_FromBase(this);
  return Dev_USART_Port_MapStatus(
    Platform_UART_Receive(self->instance, buf, size, timeout)
  );
}

static Dev_USART_Status Dev_USART_Port_Transmit(
  usart_base_t *this,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  usart_port_t *self = Dev_USART_Port_FromBase(this);
  return Dev_USART_Port_MapStatus(
    Platform_UART_Transmit(self->instance, data, size, timeout)
  );
}

static Dev_USART_Status Dev_USART_Port_ReceiveIT(
  usart_base_t *this,
  uint8_t *pdata,
  uint16_t size
) {
  usart_port_t *self = Dev_USART_Port_FromBase(this);
  return Dev_USART_Port_MapStatus(
    Platform_UART_Receive_IT(self->instance, pdata, size)
  );
}

static Dev_USART_Status Dev_USART_Port_TransmitIT(
  usart_base_t *this,
  const uint8_t *pdata,
  uint16_t size
) {
  usart_port_t *self = Dev_USART_Port_FromBase(this);
  return Dev_USART_Port_MapStatus(
    Platform_UART_Transmit_IT(self->instance, pdata, size)
  );
}

static Dev_USART_Status Dev_USART_Port_ReceiveDMA(
  usart_base_t *this,
  uint8_t *pdata,
  uint16_t size
) {
  usart_port_t *self = Dev_USART_Port_FromBase(this);
  return Dev_USART_Port_MapStatus(
    Platform_UART_Receive_DMA(self->instance, pdata, size)
  );
}

static Dev_USART_Status Dev_USART_Port_TransmitDMA(
  usart_base_t *this,
  const uint8_t *pdata,
  uint16_t size
) {
  usart_port_t *self = Dev_USART_Port_FromBase(this);
  return Dev_USART_Port_MapStatus(
    Platform_UART_Transmit_DMA(self->instance, pdata, size)
  );
}

static const usart_ops_t usart_ops_port = {
  .receive      = Dev_USART_Port_Receive,
  .transmit     = Dev_USART_Port_Transmit,
  .receive_it   = Dev_USART_Port_ReceiveIT,
  .transmit_it  = Dev_USART_Port_TransmitIT,
  .receive_dma  = Dev_USART_Port_ReceiveDMA,
  .transmit_dma = Dev_USART_Port_TransmitDMA,
};

static void Dev_USART_Port_OnEvent(
  Platform_UART_Instance instance,
  const Platform_UART_Event *event,
  void *context
) {
  usart_port_t *self = (usart_port_t *)context;
  assert(self && event && instance == self->instance);

  switch (event->type) {
    case PLATFORM_UART_EVENT_TX_CPLT:
      Dev_USART_Base_OnTxCplt(&self->usart_base);
      break;
    case PLATFORM_UART_EVENT_TX_HALF:
      Dev_USART_Base_OnTxHalfCplt(&self->usart_base);
      break;
    case PLATFORM_UART_EVENT_RX_CPLT:
      Dev_USART_Base_OnRxCplt(&self->usart_base);
      break;
    case PLATFORM_UART_EVENT_RX_HALF:
      Dev_USART_Base_OnRxHalfCplt(&self->usart_base);
      break;
    case PLATFORM_UART_EVENT_ERROR:
      Dev_USART_Base_OnError(&self->usart_base);
      break;
    case PLATFORM_UART_EVENT_ABORT_CPLT:
      Dev_USART_Base_OnAbortCplt(&self->usart_base);
      break;
    case PLATFORM_UART_EVENT_ABORT_TX_CPLT:
      Dev_USART_Base_OnAbortTxCplt(&self->usart_base);
      break;
    case PLATFORM_UART_EVENT_ABORT_RX_CPLT:
      Dev_USART_Base_OnAbortRxCplt(&self->usart_base);
      break;
    default:
      assert(0);
      break;
  }
}

Platform_StatusTypeDef Dev_USART_Port_Init(
  usart_port_t *this,
  const char *name,
  Platform_UART_Instance instance
) {
  assert(this && name);

  Dev_USART_Base_Init(&this->usart_base, name, &usart_ops_port);
  this->instance = instance;

  platform_uart_callbacks_t callbacks = {
    .callback = Dev_USART_Port_OnEvent,
    .context  = this,
  };

  return Platform_UART_RegisterCallbacks(instance, &callbacks);
}
