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
 * @brief 从 UART 基类视图还原具体端口对象。
 * @param uart_h 指向 uart_port_t 内嵌 uart_base_t 的借用指针，不得为 NULL。
 * @return 与 uart_h 同地址的具体端口对象借用指针；调用成功时不返回 NULL。
 * @pre uart_h 必须确实指向 uart_port_t 的首成员 uart_base_t。
 * @post 只建立类型视图，不复制或修改对象，也不转移所有权。
 * @note 不访问外设且不会阻塞；返回指针的生命周期与 uart_h 所属对象一致。
 */
static uart_port_t *dev_uart_port_frombase(uart_base_t *uart_h) {
  assert(uart_h);
  return (uart_port_t *)uart_h;
}

/**
 * @brief 把 Platform UART 状态映射为设备 UART 状态。
 * @param status Platform UART 状态码，应为已定义枚举值。
 * @retval DEV_UART_OK Platform 状态为 OK。
 * @retval DEV_UART_BUSY Platform 状态为 BUSY。
 * @retval DEV_UART_TIMEOUT Platform 状态为 TIMEOUT。
 * @retval DEV_UART_ERROR Platform 状态为 ERROR、INVALID_ARGUMENT、UNSUPPORTED 或未定义值。
 * @post 纯映射，不修改对象、外设或全局状态。
 * @warning 未定义枚举值属于编程错误：断言启用时触发 assert，关闭断言后返回 DEV_UART_ERROR。
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
 * @param uart_h 已初始化端口对象的 uart_base_t 借用视图，不得为 NULL。
 * @param buf 接收缓冲区，至少可写 size 字节；所有权归调用方。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @param timeout 超时时间，单位 ms；可为 PLATFORM_TIMEOUT_INFINITE。
 * @retval DEV_UART_OK 超时前收满 size 字节。
 * @retval DEV_UART_BUSY UART 已被其他传输占用。
 * @retval DEV_UART_TIMEOUT 到期仍未收满。
 * @retval DEV_UART_ERROR 参数或实例非法、功能不支持或后端失败。
 * @pre uart_h 对应实例已由板级初始化，且调用方拥有该 UART 当前传输的使用权。
 * @post 成功时 buf 前 size 字节有效；返回后本函数不再保留 buf。
 * @note 同步阻塞，禁止在 ISR 调用；同一 UART 实例不得与其他收发操作并发。
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
 * @param uart_h 已初始化端口对象的 uart_base_t 借用视图，不得为 NULL。
 * @param data 只读发送缓冲区，至少包含 size 字节；所有权归调用方。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @param timeout 超时时间，单位 ms；可为 PLATFORM_TIMEOUT_INFINITE。
 * @retval DEV_UART_OK 超时前发送完全部字节。
 * @retval DEV_UART_BUSY UART 已被其他传输占用。
 * @retval DEV_UART_TIMEOUT 到期仍未发送完。
 * @retval DEV_UART_ERROR 参数或实例非法、功能不支持或后端失败。
 * @pre uart_h 对应实例已由板级初始化，且调用方拥有该 UART 当前传输的使用权。
 * @post 成功时 data 的全部字节已交给 UART；返回后本函数不再保留 data。
 * @note 同步阻塞，禁止在 ISR 调用；同一 UART 实例不得与其他收发操作并发。
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
 * @param uart_h 已初始化端口对象的 uart_base_t 借用视图，不得为 NULL。
 * @param pdata 中断接收缓冲区，至少可写 size 字节；所有权归调用方。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @retval DEV_UART_OK 接收已挂起，完成或错误将通过回调通知。
 * @retval DEV_UART_BUSY UART 已被其他传输占用。
 * @retval DEV_UART_TIMEOUT 后端返回超时；本 IT 接口通常不产生该结果。
 * @retval DEV_UART_ERROR 参数或实例非法、功能不支持或后端失败。
 * @pre uart_h 对应实例已初始化，且已注册的接收完成/错误回调可处理本次传输。
 * @post 成功返回不表示已收到数据；pdata 在接收完成、错误或中止前不得被改写。
 * @note 非阻塞；pdata 的生命周期必须覆盖到对应完成或错误回调，重新挂接收由设备驱动负责。
 * @warning 同一 UART 实例不得并发注册另一收发；缓冲区所有权不转移给本适配器。
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
 * @param uart_h 已初始化端口对象的 uart_base_t 借用视图，不得为 NULL。
 * @param pdata 只读发送缓冲区，至少包含 size 字节；所有权归调用方。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @retval DEV_UART_OK 发送已挂起，完成或错误将通过回调通知。
 * @retval DEV_UART_BUSY UART 已被其他传输占用。
 * @retval DEV_UART_TIMEOUT 后端返回超时；本 IT 接口通常不产生该结果。
 * @retval DEV_UART_ERROR 参数或实例非法、功能不支持或后端失败。
 * @pre uart_h 对应实例已初始化，且已注册的发送完成/错误回调可处理本次传输。
 * @post 成功返回时数据尚未全部发出；pdata 在发送完成、错误或中止前不得被改写。
 * @note 非阻塞；pdata 的生命周期必须覆盖到对应完成或错误回调。
 * @warning 同一 UART 实例不得并发注册另一收发；缓冲区所有权不转移给本适配器。
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
 * @param uart_h 已初始化端口对象的 uart_base_t 借用视图，不得为 NULL。
 * @param pdata DMA 接收缓冲区，至少可写 size 字节；所有权归调用方。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @retval DEV_UART_OK 接收已挂起，完成或错误将通过回调通知。
 * @retval DEV_UART_BUSY UART 已被其他传输占用。
 * @retval DEV_UART_TIMEOUT 后端返回超时；本 DMA 接口通常不产生该结果。
 * @retval DEV_UART_ERROR 参数或实例非法、DMA 不可用或后端失败。
 * @pre uart_h 对应实例已初始化，且已注册的接收回调可处理本次 DMA 传输。
 * @post 成功返回不表示已收到数据；pdata 在完成、错误或中止前不得被改写。
 * @note 半完成回调时只能访问已完成的一半数据；全完成前 buffer 必须保持有效。
 * @warning 同一 UART 实例不得并发注册另一收发；缓冲区所有权不转移给本适配器。
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
 * @param uart_h 已初始化端口对象的 uart_base_t 借用视图，不得为 NULL。
 * @param pdata 只读 DMA 发送缓冲区，至少包含 size 字节；所有权归调用方。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @retval DEV_UART_OK 发送已挂起，完成或错误将通过回调通知。
 * @retval DEV_UART_BUSY UART 已被其他传输占用。
 * @retval DEV_UART_TIMEOUT 后端返回超时；本 DMA 接口通常不产生该结果。
 * @retval DEV_UART_ERROR 参数或实例非法、DMA 不可用或后端失败。
 * @pre uart_h 对应实例已初始化，且已注册的发送完成/错误回调可处理本次 DMA 传输。
 * @post 成功返回时数据尚未全部发出；传输完成、错误或中止前不得改写 pdata。
 * @note 非阻塞；pdata 的生命周期必须覆盖到对应完成或错误回调。
 * @warning 同一 UART 实例不得并发注册另一收发；缓冲区所有权不转移给本适配器。
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
 * @param instance 产生事件的 Platform UART 实例，必须等于 context 所属端口的实例。
 * @param event 事件描述，仅在本回调期间有效；函数不会保存该指针。
 * @param context 注册时传入的 uart_port_t 借用指针，不得为 NULL。
 * @pre context 指向已完成 Dev_UART_Port_Init 的端口，且 event 由匹配的 instance 产生。
 * @post 已配置的对应 uart_base 回调被同步调用一次；event 与 context 的所有权不变。
 * @note 在 Platform UART 中断上下文执行，禁止阻塞、分配内存或执行长耗时操作。
 * @warning instance 不匹配或事件类型未知属于编程错误并触发 assert；event->error 当前不转发。
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
 * @brief 初始化 UART 端口适配器并注册 Platform 事件回调。
 * @param uart_h 待初始化的端口对象，不得为 NULL；调用方保留所有权。
 * @param name 调试名称，不得为 NULL；指针会被保存，必须在对象生命周期内有效。
 * @param instance 已由板级配置启用的 UART 实例。
 * @retval PLATFORM_STATUS_OK 内嵌基类已初始化且 Platform 回调注册成功。
 * @retval PLATFORM_STATUS_BUSY 该 UART 实例已注册其他 Platform 回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例未启用、回调集合非法或实例编号非法。
 * @pre UART 硬件已由 CubeMX/Core 完成初始化，且调用发生在任务和中断使用该端口之前。
 * @post 无论注册结果如何，内嵌 uart_base 已按 name 初始化、旧回调已清空且 instance 已保存；
 *       注册成功时本端口成为该 Platform UART 回调槽的唯一所有者。
 * @note 不分配动态内存且不主动启动传输；非线程安全，不得与同一端口的初始化或事件并发。
 * @warning 注册失败时不回滚对象字段，调用方不得依赖该端口的 Platform 事件转发。
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
