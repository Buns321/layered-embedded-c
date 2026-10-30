/**
 * @file uart_contract_test.c
 * @brief UART Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_uart_backend.h"
#include "platform_uart.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static platform_status_t s_transmit_status;
static uint32_t s_backend_error;
static uint32_t s_event_count;
static platform_uart_event_t s_last_event;

/**
 * @brief 查询测试替身接受的 UART 实例。
 * @param uart UART 实例编号；测试替身只接受 PLATFORM_UART_1。
 * @retval true uart 等于 PLATFORM_UART_1。
 * @retval false uart 为其他枚举值。
 * @post 只比较枚举值，不解析 HAL 句柄、不访问外设，也不修改全局状态。
 * @note 这是测试替身；生产后端的启用和句柄映射规则不由本函数模拟。
 */
bool platform_uart_backend_is_valid(platform_uart_instance_t uart) {
  return uart == PLATFORM_UART_1;
}

/**
 * @brief 模拟阻塞接收并固定报告成功，但不写入缓冲区。
 * @param uart UART 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param data 接收缓冲区；本替身不读取、不写入，也不保存该指针。
 * @param size 接收字节数；本替身忽略该参数，不检查范围。
 * @param timeout 超时时间，单位 ms；本替身忽略该参数且不等待。
 * @retval PLATFORM_STATUS_OK 固定返回，未接收数据，data 内容不变。
 * @post data 内容和长度均不变，不访问 UART 寄存器或中断状态。
 * @note 测试替身不会返回生产后端声明的 ERROR、BUSY、TIMEOUT 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_uart_backend_receive(
  platform_uart_instance_t uart,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)uart;
  (void)data;
  (void)size;
  (void)timeout;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 返回测试注入的阻塞发送状态。
 * @param uart UART 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param data 发送缓冲区；本替身不读取，也不保存该指针。
 * @param size 发送字节数；本替身忽略该参数，不检查范围。
 * @param timeout 超时时间，单位 ms；本替身忽略该参数且不等待。
 * @return s_transmit_status 的当前值；测试只注入 PLATFORM_STATUS_OK 或
 *         PLATFORM_STATUS_ERROR，本替身不发送数据。
 * @post s_transmit_status 和 data 指向的内容均不变，不访问 UART 寄存器。
 * @note 这是测试替身；生产后端还可能返回 BUSY、TIMEOUT 或 INVALID_ARGUMENT。
 */
platform_status_t platform_uart_backend_transmit(
  platform_uart_instance_t uart,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)uart;
  (void)data;
  (void)size;
  (void)timeout;
  return s_transmit_status;
}

/**
 * @brief 模拟启动 UART 中断接收并固定报告成功。
 * @param uart UART 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param data 接收缓冲区；本替身不读取、不写入，也不保存该指针。
 * @param size 接收字节数；本替身忽略该参数，不检查范围。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动中断接收。
 * @post 不修改缓冲区、NVIC 或 UART 接收状态，也不会产生完成或错误事件。
 * @note 测试替身不会返回生产后端声明的 INVALID_ARGUMENT、BUSY 或 ERROR 分支。
 */
platform_status_t platform_uart_backend_receive_it(
  platform_uart_instance_t uart,
  uint8_t *data,
  uint16_t size
) {
  (void)uart;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 UART 中断发送并固定报告成功。
 * @param uart UART 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param data 发送缓冲区；本替身不读取，也不保存该指针。
 * @param size 发送字节数；本替身忽略该参数，不检查范围。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动中断发送。
 * @post 不修改缓冲区、NVIC 或 UART 发送状态，也不会产生 TX 完成事件。
 * @note 测试替身不会返回生产后端声明的 INVALID_ARGUMENT、BUSY 或 ERROR 分支。
 */
platform_status_t platform_uart_backend_transmit_it(
  platform_uart_instance_t uart,
  const uint8_t *data,
  uint16_t size
) {
  (void)uart;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 UART DMA 接收并固定报告成功。
 * @param uart UART 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param data 接收缓冲区；本替身不读取、不写入，也不保存该指针。
 * @param size 接收字节数；本替身忽略该参数，不检查范围。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动 DMA 接收。
 * @post 不修改缓冲区、DMA 控制器或 UART 状态，也不会产生半完成或完成事件。
 * @note 测试替身不会返回生产后端声明的 INVALID_ARGUMENT、BUSY 或 ERROR 分支。
 */
platform_status_t platform_uart_backend_receive_dma(
  platform_uart_instance_t uart,
  uint8_t *data,
  uint16_t size
) {
  (void)uart;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟启动 UART DMA 发送并固定报告成功。
 * @param uart UART 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @param data 发送缓冲区；本替身不读取，也不保存该指针。
 * @param size 发送字节数；本替身忽略该参数，不检查范围。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动 DMA 发送。
 * @post 不修改缓冲区、DMA 控制器或 UART 状态，也不会产生 TX 完成事件。
 * @note 测试替身不会返回生产后端声明的 INVALID_ARGUMENT、BUSY 或 ERROR 分支。
 */
platform_status_t platform_uart_backend_transmit_dma(
  platform_uart_instance_t uart,
  const uint8_t *data,
  uint16_t size
) {
  (void)uart;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟中止 UART 接收并固定报告成功。
 * @param uart UART 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @retval PLATFORM_STATUS_OK 固定返回，未中止任何真实接收。
 * @post 不修改 RX 标志、HAL 错误码或 UART 状态。
 * @note 测试替身不会返回生产后端声明的 ERROR、BUSY 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_uart_backend_abort_receive(
  platform_uart_instance_t uart
) {
  (void)uart;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟中止 UART 发送并固定报告成功。
 * @param uart UART 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @retval PLATFORM_STATUS_OK 固定返回，未中止任何真实发送。
 * @post 不修改发送状态或 HAL 错误码。
 * @note 测试替身不会返回生产后端声明的 ERROR、BUSY 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_uart_backend_abort_transmit(
  platform_uart_instance_t uart
) {
  (void)uart;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟中止 UART 双向传输并固定报告成功。
 * @param uart UART 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @retval PLATFORM_STATUS_OK 固定返回，未中止任何真实发送或接收。
 * @post 不修改 UART 状态、RX 标志或 HAL 错误码。
 * @note 测试替身不会返回生产后端声明的 ERROR、BUSY 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_uart_backend_abort(
  platform_uart_instance_t uart
) {
  (void)uart;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 返回测试注入的 UART 错误位。
 * @param uart UART 实例编号；本替身忽略该参数，不校验实例或句柄。
 * @return s_backend_error 的当前值；测试直接写入 platform_uart_error_t 位掩码，
 *         本替身不添加 UNKNOWN 位，也不映射 HAL 错误。
 * @post 只读取 s_backend_error，不清除错误，也不改变发送或接收状态。
 * @note 这是测试替身；生产后端的错误映射和非法实例处理不由本函数模拟。
 */
uint32_t platform_uart_backend_get_error(platform_uart_instance_t uart) {
  (void)uart;
  return s_backend_error;
}

/**
 * @brief 记录 UART 接收、发送或错误事件。
 * @param uart 事件所属 UART；测试要求必须为 PLATFORM_UART_1。
 * @param event 事件内容指针；只在本次回调期间读取并复制。
 * @param context 回调上下文；测试要求必须等于 &s_event_count。
 * @pre uart 必须为 PLATFORM_UART_1 且 context 必须为 &s_event_count，否则 assert 终止进程；event 必须指向调用期间有效的事件对象，否则解引用未定义。
 * @post s_event_count 加 1，s_last_event 被替换为 event 的完整副本。
 * @note 测试桩同步执行，不阻塞、不分配内存，也不保留 event 指针。
 * @warning 断言失败会终止进程，而不是返回错误状态。
 */
static void uart_callback(
  platform_uart_instance_t uart,
  const platform_uart_event_t *event,
  void *context
) {
  assert(uart == PLATFORM_UART_1);
  assert(context == &s_event_count);
  s_event_count++;
  s_last_event = *event;
}

/**
 * @brief 验证 UART 参数校验、收发和事件分发。
 * @post 成功路径留下 2 次事件回调观测，注册表保持 UART1 回调已占用状态。
 * @note 按顺序验证空缓冲区、后端发送失败、成功清除错误、重复注册 BUSY、错误事件和完成事件。
 * @warning 任一 assert 失败都会终止测试进程；静态后端状态可能保留部分注入值。
 */
static void test_uart_contract(void) {
  s_transmit_status = PLATFORM_STATUS_OK;
  s_backend_error = PLATFORM_UART_ERROR_NONE;
  s_event_count = 0U;

  uint8_t byte = 0U;
  assert(Platform_UART_Transmit(PLATFORM_UART_1, NULL, 1U, 10U) ==
         PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_UART_GetLastError(PLATFORM_UART_1) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);

  s_transmit_status = PLATFORM_STATUS_ERROR;
  assert(Platform_UART_Transmit(PLATFORM_UART_1, &byte, 1U, 10U) ==
         PLATFORM_STATUS_ERROR);
  assert((Platform_UART_GetLastError(PLATFORM_UART_1) &
          PLATFORM_ERROR_HAL) != 0U);

  s_transmit_status = PLATFORM_STATUS_OK;
  assert(Platform_UART_Transmit(PLATFORM_UART_1, &byte, 1U, 10U) ==
         PLATFORM_STATUS_OK);
  assert(Platform_UART_GetLastError(PLATFORM_UART_1) ==
         PLATFORM_ERROR_NONE);

  const platform_uart_callback_binding_t callbacks = {
    .callback = uart_callback,
    .context = &s_event_count,
  };
  assert(Platform_UART_RegisterCallback(PLATFORM_UART_1, &callbacks) ==
         PLATFORM_STATUS_OK);
  assert(Platform_UART_RegisterCallback(PLATFORM_UART_1, &callbacks) ==
         PLATFORM_STATUS_BUSY);

  s_backend_error = PLATFORM_UART_ERROR_OVERRUN;
  platform_uart_dispatch_event(PLATFORM_UART_1, PLATFORM_UART_EVENT_ERROR);
  assert(s_event_count == 1U);
  assert(s_last_event.type == PLATFORM_UART_EVENT_ERROR);
  assert(s_last_event.error == PLATFORM_UART_ERROR_OVERRUN);

  platform_uart_dispatch_event(PLATFORM_UART_1, PLATFORM_UART_EVENT_TX_COMPLETE);
  assert(s_event_count == 2U);
  assert(s_last_event.type == PLATFORM_UART_EVENT_TX_COMPLETE);
  assert(s_last_event.error == PLATFORM_UART_ERROR_NONE);

  assert((Platform_UART_GetLastError((platform_uart_instance_t)99U) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);
}

/**
 * @brief 运行 UART 公共契约测试。
 * @return 用例完成后返回 0。
 * @post 成功路径完成参数校验、状态传播、错误快照、回调注册和事件分发验证。
 * @note 任一 assert 失败都会立即终止测试进程，不会返回失败状态。
 * @warning 测试使用同步 UART 后端桩，不验证真实中断上下文、DMA 或硬件时序。
 */
int main(void) {
  test_uart_contract();
  return 0;
}
