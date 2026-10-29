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

/**
 * @brief 把 UART base 句柄还原为端口对象。
 */
static uart_port_t *dev_uart_port_frombase(uart_base_t *uart_h) {
  assert(uart_h);
  return (uart_port_t *)uart_h;
}

/**
 * @brief 把 Platform UART 状态映射为设备状态。
 */
static dev_uart_status_t dev_uart_port_mapstatus(
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

/**
 * @brief 转发阻塞接收请求。
 */
static dev_uart_status_t dev_uart_port_receive(
  uart_base_t *uart_h,
  uint8_t *buf,
  uint16_t size,
  uint32_t timeout
) {
  uart_port_t *uart_port_h = dev_uart_port_frombase(uart_h);
  return dev_uart_port_mapstatus(
    Platform_UART_Receive(uart_port_h->instance, buf, size, timeout)
  );
}

/**
 * @brief 转发阻塞发送请求。
 */
static dev_uart_status_t dev_uart_port_transmit(
  uart_base_t *uart_h,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  uart_port_t *uart_port_h = dev_uart_port_frombase(uart_h);
  return dev_uart_port_mapstatus(
    Platform_UART_Transmit(uart_port_h->instance, data, size, timeout)
  );
}

/**
 * @brief 注册中断接收请求。
 */
static dev_uart_status_t dev_uart_port_receiveit(
  uart_base_t *uart_h,
  uint8_t *pdata,
  uint16_t size
) {
  uart_port_t *uart_port_h = dev_uart_port_frombase(uart_h);
  return dev_uart_port_mapstatus(
    Platform_UART_ReceiveIT(uart_port_h->instance, pdata, size)
  );
}

/**
 * @brief 注册中断发送请求。
 */
static dev_uart_status_t dev_uart_port_transmitit(
  uart_base_t *uart_h,
  const uint8_t *pdata,
  uint16_t size
) {
  uart_port_t *uart_port_h = dev_uart_port_frombase(uart_h);
  return dev_uart_port_mapstatus(
    Platform_UART_TransmitIT(uart_port_h->instance, pdata, size)
  );
}

/**
 * @brief 注册 DMA 接收请求。
 */
static dev_uart_status_t dev_uart_port_receivedma(
  uart_base_t *uart_h,
  uint8_t *pdata,
  uint16_t size
) {
  uart_port_t *uart_port_h = dev_uart_port_frombase(uart_h);
  return dev_uart_port_mapstatus(
    Platform_UART_ReceiveDMA(uart_port_h->instance, pdata, size)
  );
}

/**
 * @brief 注册 DMA 发送请求。
 */
static dev_uart_status_t dev_uart_port_transmitdma(
  uart_base_t *uart_h,
  const uint8_t *pdata,
  uint16_t size
) {
  uart_port_t *uart_port_h = dev_uart_port_frombase(uart_h);
  return dev_uart_port_mapstatus(
    Platform_UART_TransmitDMA(uart_port_h->instance, pdata, size)
  );
}

static const uart_ops_t uart_ops_port = {
  .receive      = dev_uart_port_receive,
  .transmit     = dev_uart_port_transmit,
  .receive_it   = dev_uart_port_receiveit,
  .transmit_it  = dev_uart_port_transmitit,
  .receive_dma  = dev_uart_port_receivedma,
  .transmit_dma = dev_uart_port_transmitdma,
};

/**
 * @brief 把 Platform UART 事件转发给设备回调。
 */
static void dev_uart_port_onevent(
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

/**
 * @brief 初始化 UART 适配器并注册 Platform 回调。
 * @param uart_h 对象。
 * @param name 名称。
 * @param instance UART 实例身份。
 * @note Platform 回调会把事件转发到 uart_base 的回调集合。
 * @retval PLATFORM_STATUS_OK 初始化和回调注册成功。
 * @retval PLATFORM_STATUS_BUSY 该 UART 实例已注册 Platform 回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 参数或实例非法。
 */
platform_status_t Dev_UART_Port_Init(
  uart_port_t *uart_h,
  const char *name,
  platform_uart_instance_t instance
) {
  assert(uart_h && name);

  Dev_UART_Base_Init(&uart_h->uart_base, name, &uart_ops_port);
  uart_h->instance = instance;

  platform_uart_callback_binding_t callbacks = {
    .callback = dev_uart_port_onevent,
    .context  = uart_h,
  };

  return Platform_UART_RegisterCallback(instance, &callbacks);
}
