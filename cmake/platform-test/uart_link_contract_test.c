/**
 * @file uart_link_contract_test.c
 * @brief UART 文本行链路契约测试。
 * @ingroup tests
 */

#include "uart_link.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static dev_uart_callbacks_t s_callbacks;
static dev_uart_status_t s_receive_status = DEV_UART_OK;
static uint32_t s_receive_calls;
static uint32_t s_tx_complete_calls;
static uint32_t s_handler_calls;
static uint32_t s_handler_time_ms;
static char s_last_line[16];
static bool s_accept_line = true;

/**
 * @brief 记录并接受或拒绝一条测试文本行。
 * @param line 非空且以 NUL 结尾的文本行，仅本次调用期间有效。
 * @param now_ms 测试行时间戳，单位 ms。
 * @param context 必须等于 &s_handler_calls。
 * @return s_accept_line 的当前值。
 * @pre line 和 context 符合链路回调契约。
 * @post 保存最后一行和时间，并递增调用计数。
 * @note 测试回调不阻塞、不分配内存。
 */
static bool uart_link_test_handler(
  char *line,
  uint32_t now_ms,
  void *context
) {
  assert(line && context == &s_handler_calls);
  s_handler_calls++;
  s_handler_time_ms = now_ms;
  size_t length = strlen(line);
  if (length >= sizeof(s_last_line)) {
    length = sizeof(s_last_line) - 1u;
  }
  memcpy(s_last_line, line, length);
  s_last_line[length] = '\0';
  return s_accept_line;
}

/**
 * @brief UART 基类初始化测试替身。
 * @param uart_h UART 基对象；测试不检查。
 * @param name 名称；测试不保存。
 * @param ops 操作表；测试不保存。
 * @post 函数体为空。
 * @note 仅用于满足链接依赖。
 */
void Dev_UART_Base_Init(
  uart_base_t *uart_h,
  const char *name,
  const uart_ops_t *ops
) {
  (void)uart_h;
  (void)name;
  (void)ops;
}

/**
 * @brief 阻塞接收测试替身。
 * @param uart_h UART 基对象；测试忽略。
 * @param buf 接收缓冲区；测试忽略。
 * @param size 字节数；测试忽略。
 * @param timeout 超时时间，单位 ms；测试忽略。
 * @return 固定返回 DEV_UART_OK。
 * @pre 无。
 * @post 不修改参数。
 * @note 仅用于满足链接依赖。
 */
dev_uart_status_t Dev_UART_Base_Receive(
  uart_base_t *uart_h,
  uint8_t *buf,
  uint16_t size,
  uint32_t timeout
) {
  (void)uart_h;
  (void)buf;
  (void)size;
  (void)timeout;
  return DEV_UART_OK;
}

/**
 * @brief 阻塞发送测试替身。
 * @param uart_h UART 基对象；测试忽略。
 * @param data 发送缓冲区；测试忽略。
 * @param size 字节数；测试忽略。
 * @param timeout 超时时间，单位 ms；测试忽略。
 * @return 固定返回 DEV_UART_OK。
 * @pre 无。
 * @post 不修改参数。
 * @note 仅用于满足链接依赖。
 */
dev_uart_status_t Dev_UART_Base_Transmit(
  uart_base_t *uart_h,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)uart_h;
  (void)data;
  (void)size;
  (void)timeout;
  return DEV_UART_OK;
}

/**
 * @brief 中断接收启动测试替身。
 * @param uart_h UART 基对象；测试忽略。
 * @param pdata 接收缓冲区；测试忽略。
 * @param size 字节数；测试忽略。
 * @return 当前 s_receive_status。
 * @pre 无。
 * @post s_receive_calls 加一。
 * @note 用于验证 BUSY 后自动重挂。
 */
dev_uart_status_t Dev_UART_ReceiveIT(
  uart_base_t *uart_h,
  uint8_t *pdata,
  uint16_t size
) {
  (void)uart_h;
  (void)pdata;
  (void)size;
  s_receive_calls++;
  return s_receive_status;
}

/**
 * @brief 中断发送启动测试替身。
 * @param uart_h UART 基对象；测试忽略。
 * @param pdata 发送缓冲区；测试忽略。
 * @param size 字节数；测试忽略。
 * @return 固定返回 DEV_UART_OK。
 * @pre 无。
 * @post 不修改参数。
 * @note 仅用于满足链接依赖。
 */
dev_uart_status_t Dev_UART_TransmitIT(
  uart_base_t *uart_h,
  const uint8_t *pdata,
  uint16_t size
) {
  (void)uart_h;
  (void)pdata;
  (void)size;
  return DEV_UART_OK;
}

/**
 * @brief DMA 接收启动测试替身。
 * @param uart_h UART 基对象；测试忽略。
 * @param pdata 接收缓冲区；测试忽略。
 * @param size 字节数；测试忽略。
 * @return 固定返回 DEV_UART_OK。
 * @pre 无。
 * @post 不修改参数。
 * @note 仅用于满足链接依赖。
 */
dev_uart_status_t Dev_UART_ReceiveDMA(
  uart_base_t *uart_h,
  uint8_t *pdata,
  uint16_t size
) {
  (void)uart_h;
  (void)pdata;
  (void)size;
  return DEV_UART_OK;
}

/**
 * @brief DMA 发送启动测试替身。
 * @param uart_h UART 基对象；测试忽略。
 * @param pdata 发送缓冲区；测试忽略。
 * @param size 字节数；测试忽略。
 * @return 固定返回 DEV_UART_OK。
 * @pre 无。
 * @post 不修改参数。
 * @note 仅用于满足链接依赖。
 */
dev_uart_status_t Dev_UART_TransmitDMA(
  uart_base_t *uart_h,
  const uint8_t *pdata,
  uint16_t size
) {
  (void)uart_h;
  (void)pdata;
  (void)size;
  return DEV_UART_OK;
}

/**
 * @brief UART 回调注册测试替身。
 * @param uart_h UART 基对象；测试忽略。
 * @param callbacks 非空回调集合。
 * @post 复制回调集合到 s_callbacks。
 * @note 用于模拟 RX/TX 中断入口。
 */
void Dev_UART_RegisterCallback(
  uart_base_t *uart_h,
  const dev_uart_callbacks_t *callbacks
) {
  (void)uart_h;
  assert(callbacks);
  s_callbacks = *callbacks;
}

/**
 * @brief TX 完成分发测试替身。
 * @param uart_h UART 基对象；测试忽略。
 * @post s_tx_complete_calls 加一。
 * @note 用于验证链路把物理 TX 完成转发到虚拟 UART。
 */
void Dev_UART_Base_OnTxComplete(uart_base_t *uart_h) {
  (void)uart_h;
  s_tx_complete_calls++;
}

/**
 * @brief UART 端口初始化测试替身。
 * @param uart_h 端口对象；测试忽略。
 * @param name 名称；测试忽略。
 * @param instance UART 实例；测试忽略。
 * @return 固定返回 PLATFORM_STATUS_OK。
 * @pre 无。
 * @post 不修改参数。
 * @note 仅用于满足链接依赖。
 */
platform_status_t Dev_UART_Port_Init(
  uart_port_t *uart_h,
  const char *name,
  platform_uart_instance_t instance
) {
  (void)uart_h;
  (void)name;
  (void)instance;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 通过真实中断回调入口注入一个 RX 字节。
 * @param link_h 已初始化链路，不得为 NULL。
 * @param value 待注入字节。
 * @pre s_callbacks.rx_complete_cb 和 context 已由 Init 注册。
 * @post 回调读取 link_h->rx_byte 并尝试重挂接收。
 * @note 模拟 UART RX 完成 ISR。
 */
static void uart_link_test_inject_byte(uart_link_t *link_h, char value) {
  assert(link_h && s_callbacks.rx_complete_cb);
  link_h->rx_byte = (uint8_t)value;
  s_callbacks.rx_complete_cb(&link_h->uart_view, s_callbacks.context);
}

/**
 * @brief 通过真实中断回调入口注入以 NUL 结尾的文本。
 * @param link_h 已初始化链路，不得为 NULL。
 * @param text 非空测试文本，不包含结尾 NUL。
 * @pre 环形缓冲足以容纳全部字节。
 * @post 每个字节均经过 RX 完成回调。
 * @note 模拟连续 UART RX 中断。
 */
static void uart_link_test_inject_text(uart_link_t *link_h, const char *text) {
  assert(link_h && text);
  for (const char *cursor = text; *cursor != '\0'; cursor++) {
    uart_link_test_inject_byte(link_h, *cursor);
  }
}

/**
 * @brief 初始化测试链路并清零全局替身状态。
 * @param link_h 输出链路对象，不得为 NULL。
 * @param rx_buffer 非空 RX 环形缓冲。
 * @param rx_capacity_bytes RX 缓冲容量，单位 byte。
 * @param line_buffer 非空文本行缓冲。
 * @param line_capacity_bytes 文本行容量，单位 byte。
 * @pre 所有缓冲在测试期间保持有效。
 * @post 链路成功初始化，全局计数和替身状态复位。
 * @note 不访问真实 UART。
 */
static void uart_link_test_init(
  uart_link_t *link_h,
  uint8_t *rx_buffer,
  uint16_t rx_capacity_bytes,
  char *line_buffer,
  uint16_t line_capacity_bytes
) {
  memset(&s_callbacks, 0, sizeof(s_callbacks));
  s_receive_status = DEV_UART_OK;
  s_receive_calls = 0u;
  s_tx_complete_calls = 0u;
  s_handler_calls = 0u;
  s_handler_time_ms = 0u;
  s_last_line[0] = '\0';
  s_accept_line = true;

  uart_link_config_t config = {
    .name = "uart-link-test",
    .instance = PLATFORM_UART_1,
    .rx_buffer = rx_buffer,
    .rx_capacity_bytes = rx_capacity_bytes,
    .line_buffer = line_buffer,
    .line_capacity_bytes = line_capacity_bytes,
    .line_handler = uart_link_test_handler,
    .line_context = &s_handler_calls,
  };
  assert(Dev_UARTLink_Init(link_h, &config) == PLATFORM_STATUS_OK);
}

/**
 * @brief 验证非法配置和 BUSY 后自动重挂。
 * @post 空配置和过小缓冲被拒绝；Start 返回 BUSY 后 Process 重试并清除 pending。
 * @note 不访问真实 UART。
 */
static void test_validation_and_rearm(void) {
  uart_link_t link;
  uint8_t rx_buffer[8];
  char line_buffer[8];

  assert(Dev_UARTLink_Init(&link, NULL) == PLATFORM_STATUS_INVALID_ARGUMENT);
  uart_link_config_t invalid = {
    .name = "bad",
    .instance = PLATFORM_UART_1,
    .rx_buffer = rx_buffer,
    .rx_capacity_bytes = 1u,
    .line_buffer = line_buffer,
    .line_capacity_bytes = sizeof(line_buffer),
    .line_handler = uart_link_test_handler,
  };
  assert(Dev_UARTLink_Init(&link, &invalid) == PLATFORM_STATUS_INVALID_ARGUMENT);

  uart_link_test_init(
    &link,
    rx_buffer,
    sizeof(rx_buffer),
    line_buffer,
    sizeof(line_buffer)
  );
  s_receive_status = DEV_UART_BUSY;
  assert(Dev_UARTLink_Start(&link) == PLATFORM_STATUS_BUSY);
  assert(link.receive_pending);

  s_receive_status = DEV_UART_OK;
  Dev_UARTLink_Process(&link, 10u);
  assert(!link.receive_pending);
  assert(s_receive_calls == 2u);
}

/**
 * @brief 验证 CRLF、行回调、接受/拒绝计数和 TX 完成转发。
 * @post 两行按顺序处理，第二行被拒绝，TX 完成转发一次。
 * @note 使用公开状态和真实回调路径。
 */
static void test_line_processing_and_tx_forward(void) {
  uart_link_t link;
  uint8_t rx_buffer[64];
  char line_buffer[32];
  uart_link_test_init(
    &link,
    rx_buffer,
    sizeof(rx_buffer),
    line_buffer,
    sizeof(line_buffer)
  );
  assert(Dev_UARTLink_Start(&link) == PLATFORM_STATUS_OK);

  uart_link_test_inject_text(&link, "SET A 1\r\n");
  Dev_UARTLink_Process(&link, 20u);
  assert(s_handler_calls == 1u);
  assert(s_handler_time_ms == 20u);
  assert(strcmp(s_last_line, "SET A 1") == 0);
  assert(link.accepted_lines == 1u);

  s_accept_line = false;
  uart_link_test_inject_text(&link, "RESET\n");
  Dev_UARTLink_Process(&link, 30u);
  assert(s_handler_calls == 2u);
  assert(link.rejected_lines == 1u);

  assert(s_callbacks.tx_complete_cb);
  s_callbacks.tx_complete_cb(&link.uart_view, s_callbacks.context);
  assert(s_tx_complete_calls == 1u);
}

/**
 * @brief 验证超长行丢弃到下一个 LF。
 * @post 超长行不触发回调，后续合法行仍正常处理。
 * @note 行缓冲容量仅用于测试，不代表生产容量。
 */
static void test_line_overflow(void) {
  uart_link_t link;
  uint8_t rx_buffer[64];
  char line_buffer[6];
  uart_link_test_init(
    &link,
    rx_buffer,
    sizeof(rx_buffer),
    line_buffer,
    sizeof(line_buffer)
  );
  assert(Dev_UARTLink_Start(&link) == PLATFORM_STATUS_OK);

  uart_link_test_inject_text(&link, "TOO-LONG\nOK\n");
  Dev_UARTLink_Process(&link, 40u);
  assert(link.line_overflow_count == 1u);
  assert(s_handler_calls == 1u);
  assert(strcmp(s_last_line, "OK") == 0);
}

/**
 * @brief 验证 RX 环形缓冲满时丢弃新字节并计数。
 * @post 注入超过实际环形容量的字节后 rx_dropped_bytes 大于零。
 * @note 不在注入之间调用 Process，确保消费者不释放空间。
 */
static void test_rx_ring_overflow(void) {
  uart_link_t link;
  uint8_t rx_buffer[4];
  char line_buffer[16];
  uart_link_test_init(
    &link,
    rx_buffer,
    sizeof(rx_buffer),
    line_buffer,
    sizeof(line_buffer)
  );
  assert(Dev_UARTLink_Start(&link) == PLATFORM_STATUS_OK);

  uart_link_test_inject_text(&link, "ABCD");
  uart_link_status_t status;
  Dev_UARTLink_GetStatus(&link, &status);
  assert(status.rx_dropped_bytes > 0u);
  assert(status.rx_bytes == 4u);
}

/**
 * @brief 运行 UART 文本行链路契约测试。
 * @return 所有断言通过后返回 0。
 * @post 测试全局替身状态失效。
 * @note 任一断言失败会终止进程。
 */
int main(void) {
  test_validation_and_rearm();
  test_line_processing_and_tx_forward();
  test_line_overflow();
  test_rx_ring_overflow();
  return 0;
}
