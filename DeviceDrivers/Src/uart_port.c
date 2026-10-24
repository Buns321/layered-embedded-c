//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file uart_port.c
 * @brief Platform UART 到设备 UART 抽象层的适配实现。
 * @ingroup drivers_concrete
 */

#include "uart_port.h"
#include <assert.h>

static uart_port_t *Dev_UART_Port_FromBase(uart_base_t *uart_h) {
  assert(uart_h);
  return (uart_port_t *)uart_h;
}

static dev_uart_status_t Dev_UART_Port_MapStatus(
  platform_status_t status
) {
  switch (status) {
    case PLATFORM_STATUS_OK:               return DEV_UART_OK;
    case PLATFORM_STATUS_BUSY:             return DEV_UART_BUSY;
    case PLATFORM_STATUS_TIMEOUT:          return DEV_UART_TIMEOUT;
    case PLATFORM_STATUS_ERROR:            return DEV_UART_ERROR;
    case PLATFORM_STATUS_INVALID_ARGUMENT: return DEV_UART_ERROR;
    case PLATFORM_STATUS_UNSUPPORTED:      return DEV_UART_ERROR;
    default:                        assert(0); return DEV_UART_ERROR;
  }
}

static dev_uart_status_t Dev_UART_Port_Receive(
  uart_base_t *uart_h,
  uint8_t *buf,
  uint16_t size,
  uint32_t timeout
) {
  uart_port_t *uart_port_h = Dev_UART_Port_FromBase(uart_h);
  return Dev_UART_Port_MapStatus(
    Platform_UART_Receive(uart_port_h->instance, buf, size, timeout)
  );
}

static dev_uart_status_t Dev_UART_Port_Transmit(
  uart_base_t *uart_h,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  uart_port_t *uart_port_h = Dev_UART_Port_FromBase(uart_h);
  return Dev_UART_Port_MapStatus(
    Platform_UART_Transmit(uart_port_h->instance, data, size, timeout)
  );
}

static dev_uart_status_t Dev_UART_Port_ReceiveIT(
  uart_base_t *uart_h,
  uint8_t *pdata,
  uint16_t size
) {
  uart_port_t *uart_port_h = Dev_UART_Port_FromBase(uart_h);
  return Dev_UART_Port_MapStatus(
    Platform_UART_ReceiveIT(uart_port_h->instance, pdata, size)
  );
}

static dev_uart_status_t Dev_UART_Port_TransmitIT(
  uart_base_t *uart_h,
  const uint8_t *pdata,
  uint16_t size
) {
  uart_port_t *uart_port_h = Dev_UART_Port_FromBase(uart_h);
  return Dev_UART_Port_MapStatus(
    Platform_UART_TransmitIT(uart_port_h->instance, pdata, size)
  );
}

static dev_uart_status_t Dev_UART_Port_ReceiveDMA(
  uart_base_t *uart_h,
  uint8_t *pdata,
  uint16_t size
) {
  uart_port_t *uart_port_h = Dev_UART_Port_FromBase(uart_h);
  return Dev_UART_Port_MapStatus(
    Platform_UART_ReceiveDMA(uart_port_h->instance, pdata, size)
  );
}

static dev_uart_status_t Dev_UART_Port_TransmitDMA(
  uart_base_t *uart_h,
  const uint8_t *pdata,
  uint16_t size
) {
  uart_port_t *uart_port_h = Dev_UART_Port_FromBase(uart_h);
  return Dev_UART_Port_MapStatus(
    Platform_UART_TransmitDMA(uart_port_h->instance, pdata, size)
  );
}

static const uart_ops_t uart_ops_port = {
  .receive      = Dev_UART_Port_Receive,
  .transmit     = Dev_UART_Port_Transmit,
  .receive_it   = Dev_UART_Port_ReceiveIT,
  .transmit_it  = Dev_UART_Port_TransmitIT,
  .receive_dma  = Dev_UART_Port_ReceiveDMA,
  .transmit_dma = Dev_UART_Port_TransmitDMA,
};

static void Dev_UART_Port_OnEvent(
  platform_uart_instance_t instance,
  const platform_uart_event_t *event,
  void *context
) {
  uart_port_t *uart_port_h = (uart_port_t *)context;
  assert(uart_port_h && event && instance == uart_port_h->instance);

  switch (event->type) {
    case PLATFORM_UART_EVENT_TX_COMPLETE:
      Dev_UART_Base_OnTxComplete(&uart_port_h->uart_base);
      break;
    case PLATFORM_UART_EVENT_TX_HALF_COMPLETE:
      Dev_UART_Base_OnTxHalfComplete(&uart_port_h->uart_base);
      break;
    case PLATFORM_UART_EVENT_RX_COMPLETE:
      Dev_UART_Base_OnRxComplete(&uart_port_h->uart_base);
      break;
    case PLATFORM_UART_EVENT_RX_HALF_COMPLETE:
      Dev_UART_Base_OnRxHalfComplete(&uart_port_h->uart_base);
      break;
    case PLATFORM_UART_EVENT_ERROR:
      Dev_UART_Base_OnError(&uart_port_h->uart_base);
      break;
    case PLATFORM_UART_EVENT_ABORT_COMPLETE:
      Dev_UART_Base_OnAbortComplete(&uart_port_h->uart_base);
      break;
    case PLATFORM_UART_EVENT_ABORT_TX_COMPLETE:
      Dev_UART_Base_OnAbortTxComplete(&uart_port_h->uart_base);
      break;
    case PLATFORM_UART_EVENT_ABORT_RX_COMPLETE:
      Dev_UART_Base_OnAbortRxComplete(&uart_port_h->uart_base);
      break;
    default:
      assert(0);
      break;
  }
}

platform_status_t Dev_UART_Port_Init(
  uart_port_t *uart_h,
  const char *name,
  platform_uart_instance_t instance
) {
  assert(uart_h && name);

  Dev_UART_Base_Init(&uart_h->uart_base, name, &uart_ops_port);
  uart_h->instance = instance;

  platform_uart_callback_binding_t callbacks = {
    .callback = Dev_UART_Port_OnEvent,
    .context  = uart_h,
  };

  return Platform_UART_RegisterCallback(instance, &callbacks);
}
