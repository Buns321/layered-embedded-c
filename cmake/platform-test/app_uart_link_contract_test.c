/**
 * @file app_uart_link_contract_test.c
 * @brief F103 速通任务一 USART1 链路命令分帧契约测试。
 * @ingroup tests
 */

#include "app_uart_link.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static app_uart_command_t s_last_command;
static uint32_t s_handler_calls;
static bool s_accept_command = true;

/**
 * @brief 接收链路解析出的测试命令。
 * @param command 非空命令指针；函数按值复制整条命令，调用后不保留该指针。
 * @param context 回调上下文；断言其必须等于 &s_handler_calls。
 * @retval true s_accept_command 为 true，链路应计入接受命令。
 * @retval false s_accept_command 为 false，链路应计入拒绝命令。
 * @pre command 非空，context 等于 &s_handler_calls，且两者生命周期覆盖调用。
 * @post s_handler_calls 增加 1，s_last_command 更新为 *command。
 * @note 链路在遥测任务上下文调用此回调；任一断言失败会终止进程，
 *       函数不阻塞、不分配内存，也不访问硬件。
 */
static bool app_uart_link_testhandler(
  const app_uart_command_t *command,
  void *context
) {
  assert(command);
  assert(context == &s_handler_calls);

  s_handler_calls++;
  s_last_command = *command;
  return s_accept_command;
}

/**
 * @brief 向测试 UART 链路注入接收字节。
 * @param link_h 非空链路对象；其 rx_head 和 rx_tail 必须是有效环形索引。
 * @param text 非空且以 NUL 结尾的输入文本；NUL 本身不会写入环形缓冲。
 * @pre 环形缓冲必须为每个待注入字节保留至少一个空槽。
 * @post 每个输入字符写入 rx_ring，rx_head 按容量取模前进，text 不被修改。
 * @warning 缓冲空间不足时断言失败并终止进程；测试不检查硬件接收寄存器。
 * @note 本函数模拟 ISR 已写入的数据，但自身在测试线程同步执行。
 */
static void app_uart_link_testfeed(
  app_uart_link_t *link_h,
  const char *text
) {
  assert(link_h && text);

  for (const char *cursor = text; *cursor != '\0'; cursor++) {
    uint16_t next_head = (uint16_t)(
      (link_h->rx_head + 1u) % APP_UART_RX_RING_CAPACITY
    );
    assert(next_head != link_h->rx_tail);
    link_h->rx_ring[link_h->rx_head] = (uint8_t)*cursor;
    link_h->rx_head = next_head;
  }
}

/**
 * @brief 重置测试 UART 链路状态。
 * @param link_h 非空链路对象；调用后其先前内容全部丢弃。
 * @pre link_h 指向可写且至少 sizeof(*link_h) 字节的存储。
 * @post 链路被清零并标记为已初始化，绑定测试命令回调与上下文，
 *       最近命令状态置为 IDLE；测试全局计数和最后命令同时复位。
 * @warning 断言失败会终止进程；此函数不注册真实 UART 回调，也不访问硬件。
 */
static void app_uart_link_testreset(app_uart_link_t *link_h) {
  assert(link_h);

  memset(link_h, 0, sizeof(*link_h));
  link_h->initialized = true;
  link_h->command_handler = app_uart_link_testhandler;
  link_h->command_context = &s_handler_calls;
  link_h->last_command_status = APP_UART_LINK_COMMAND_IDLE;
  s_last_command = (app_uart_command_t){0};
  s_handler_calls = 0u;
  s_accept_command = true;
}

/**
 * @brief 验证命令组帧和换行处理。
 * @post 断言 CR 被忽略、LF 触发解析，SET 命令只回调一次并计入接受；
 *       第二条 RESET EKF 命令再次触发回调并累计接受计数。
 * @note 调用真实命令解析和链路处理代码；任一断言失败会终止进程，
 *       测试使用内存注入，不涉及 UART 中断或外设寄存器。
 */
static void test_command_framing(void) {
  app_uart_link_t link;
  app_uart_link_testreset(&link);

  app_uart_link_testfeed(&link, "SET R_ACCEL 0.15\r\n");
  App_UART_LinkProcessCommands(&link, 10u);
  assert(s_handler_calls == 1u);
  assert(s_last_command.kind == APP_UART_COMMAND_SET);
  assert(strcmp(s_last_command.name, "R_ACCEL") == 0);
  assert(s_last_command.value == 0.15f);
  assert(link.accepted_commands == 1u);
  assert(link.last_command_status == APP_UART_LINK_COMMAND_ACCEPTED);

  app_uart_link_testfeed(&link, "RESET EKF\n");
  App_UART_LinkProcessCommands(&link, 20u);
  assert(s_handler_calls == 2u);
  assert(s_last_command.kind == APP_UART_COMMAND_RESET_EKF);
  assert(link.accepted_commands == 2u);
}

/**
 * @brief 验证被拒绝和非法命令。
 * @post 断言语法正确但处理回调返回 false 的命令计入拒绝，
 *       且格式错误命令不会再次调用回调，只累计格式错误拒绝计数。
 * @note 调用真实链路处理代码；任一断言失败会终止进程，
 *       测试不检查失败命令对象的字段内容。
 */
static void test_rejected_and_invalid_commands(void) {
  app_uart_link_t link;
  app_uart_link_testreset(&link);
  s_accept_command = false;

  app_uart_link_testfeed(&link, "SET Q_ANGLE 0.0001\n");
  App_UART_LinkProcessCommands(&link, 10u);
  assert(s_handler_calls == 1u);
  assert(link.rejected_commands == 1u);
  assert(link.last_command_status == APP_UART_LINK_COMMAND_REJECTED);

  app_uart_link_testfeed(&link, "NOT A COMMAND\n");
  App_UART_LinkProcessCommands(&link, 20u);
  assert(s_handler_calls == 1u);
  assert(link.rejected_commands == 2u);
  assert(
    link.last_command_status == APP_UART_LINK_COMMAND_FORMAT_ERROR
  );
}

/**
 * @brief 验证非零环形索引和超长命令溢出处理。
 * @post 断言从 rx_head=rx_tail=200 起注入命令仍可解析；
 *       注入完整命令容量长度的文本后，命令长度清零、拒绝计数增加，
 *       最近状态变为 OVERFLOW。
 * @note 本用例没有让 rx_head 越过容量边界，回绕仅由取模索引路径间接覆盖；
 *       任一断言失败会终止进程，测试不访问 UART 硬件。
 */
static void test_overflow_and_ring_wrap(void) {
  app_uart_link_t link;
  app_uart_link_testreset(&link);
  link.rx_head = 200u;
  link.rx_tail = 200u;

  static const char crossing_text[] = "RESET EKF\n";
  app_uart_link_testfeed(&link, crossing_text);
  App_UART_LinkProcessCommands(&link, 10u);
  assert(s_handler_calls == 1u);
  assert(s_last_command.kind == APP_UART_COMMAND_RESET_EKF);

  app_uart_link_testreset(&link);
  char overlong[APP_UART_COMMAND_CAPACITY + 1u];
  memset(overlong, 'A', APP_UART_COMMAND_CAPACITY);
  overlong[APP_UART_COMMAND_CAPACITY] = '\0';

  app_uart_link_testfeed(&link, overlong);
  App_UART_LinkProcessCommands(&link, 20u);
  assert(link.command_length == 0u);
  assert(link.rejected_commands == 1u);
  assert(link.last_command_status == APP_UART_LINK_COMMAND_OVERFLOW);
}

/**
 * @brief UART 基类初始化测试替身。
 * @param uart_h UART 基类存储；公共 API 要求非 NULL，测试替身不访问。
 * @param name 调试名称；公共 API 要求非 NULL 且生命周期足够，测试替身不保存。
 * @param ops 收发操作表；公共 API 要求非 NULL，测试替身不保存。
 * @pre 公共 UART 层已完成参数校验；测试对象生命周期由调用方保证。
 * @post 函数体为空，不写入 uart_h、不绑定 ops、不改变使能状态或回调。
 * @note 本函数是测试替身；生产实现会保存绑定、置 DEV_UART_ON 并清空回调。
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
 * @brief UART 阻塞接收测试替身。
 * @param uart_h UART 基对象；公共 API 要求已初始化，测试替身忽略。
 * @param buf 调用方接收缓冲区；测试替身不写入，公共 API 要求至少可写 size 字节。
 * @param size 请求接收的字节数，公共 API 要求大于 0；测试替身忽略。
 * @param timeout 超时时间，单位 ms；测试替身忽略，不等待或阻塞。
 * @retval DEV_UART_OK 测试替身固定返回成功，不表示任何字节已接收。
 * @pre 公共 UART 层已验证参数；测试替身自身不检查 uart_h、buf 或 size。
 * @post buf 内容保持不变，不访问 UART 或 HAL，也不产生超时、忙或错误状态。
 * @note 本函数是测试替身；生产后端可能返回 BUSY、TIMEOUT 或 ERROR。
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
 * @brief UART 阻塞发送测试替身。
 * @param uart_h UART 基对象；公共 API 要求已初始化，测试替身忽略。
 * @param data 调用方只读发送缓冲区；测试替身不读取，公共 API 要求至少可读 size 字节。
 * @param size 请求发送的字节数，公共 API 要求大于 0；测试替身忽略。
 * @param timeout 超时时间，单位 ms；测试替身忽略，不等待或阻塞。
 * @retval DEV_UART_OK 测试替身固定返回成功，不表示任何字节已发送。
 * @pre 公共 UART 层已验证参数；测试替身自身不检查 uart_h、data 或 size。
 * @post data 保持不变，不访问 UART 或 HAL，也不产生超时、忙或错误状态。
 * @note 本函数是测试替身；生产后端可能返回 BUSY、TIMEOUT 或 ERROR。
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
 * @brief UART 中断接收启动测试替身。
 * @param uart_h UART 基对象；公共 API 要求已初始化，测试替身忽略。
 * @param pdata 调用方接收缓冲区；测试替身不保存或写入。
 * @param size 请求接收的字节数；测试替身忽略。
 * @retval DEV_UART_OK 测试替身固定返回成功，不启动中断接收。
 * @pre 公共 UART 层已验证参数；生产路径要求已注册回调和中断。
 * @post pdata 不被保留，不使能 NVIC、不启动 UART，也不产生后续完成事件。
 * @note 本函数是测试替身；生产后端可能返回 BUSY、ERROR 或 TIMEOUT。
 */
dev_uart_status_t Dev_UART_ReceiveIT(
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
 * @brief UART 中断发送启动测试替身。
 * @param uart_h UART 基对象；公共 API 要求已初始化，测试替身忽略。
 * @param pdata 调用方只读发送缓冲区；测试替身不保存或读取。
 * @param size 请求发送的字节数；测试替身忽略。
 * @retval DEV_UART_OK 测试替身固定返回成功，不启动中断发送。
 * @pre 公共 UART 层已验证参数；生产路径要求已注册回调和中断。
 * @post pdata 不被保留，不使能 NVIC、不启动 UART，也不产生后续完成事件。
 * @note 本函数是测试替身；生产后端可能返回 BUSY、ERROR 或 TIMEOUT。
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
 * @brief UART DMA 接收启动测试替身。
 * @param uart_h UART 基对象；公共 API 要求已初始化，测试替身忽略。
 * @param pdata 调用方接收缓冲区；测试替身不保存或写入。
 * @param size 请求接收的字节数；测试替身忽略。
 * @retval DEV_UART_OK 测试替身固定返回成功，不启动 DMA。
 * @pre 公共 UART 层已验证参数；生产路径要求 DMA 和 NVIC 已配置。
 * @post pdata 不被保留，DMA 通道不被配置，也不产生半完成、完成或错误事件。
 * @note 本函数是测试替身；生产后端可能返回 BUSY、ERROR 或 TIMEOUT。
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
 * @brief UART DMA 发送启动测试替身。
 * @param uart_h UART 基对象；公共 API 要求已初始化，测试替身忽略。
 * @param pdata 调用方只读发送缓冲区；测试替身不保存或读取。
 * @param size 请求发送的字节数；测试替身忽略。
 * @retval DEV_UART_OK 测试替身固定返回成功，不启动 DMA。
 * @pre 公共 UART 层已验证参数；生产路径要求 DMA 和 NVIC 已配置。
 * @post pdata 不被保留，DMA 通道不被配置，也不产生完成或错误事件。
 * @note 本函数是测试替身；生产后端可能返回 BUSY、ERROR 或 TIMEOUT。
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
 * @param uart_h UART 基对象；公共 API 要求已初始化，测试替身忽略。
 * @param cb 调用方回调配置；测试替身不读取、不复制。
 * @pre 公共 UART 层已确认对象可写；生产路径要求在任何事件可能触发前调用。
 * @post 函数体为空，不替换回调表，不保存 context，也不触发回调。
 * @warning 测试只验证该桩可链接并返回 void，不验证回调所有权或并发替换语义。
 * @note 本函数是测试替身；生产实现会复制整张回调表。
 */
void Dev_UART_RegisterCallback(
  uart_base_t *uart_h,
  const dev_uart_callbacks_t *cb
) {
  (void)uart_h;
  (void)cb;
}

/**
 * @brief UART 发送完成分发测试替身。
 * @param uart_h UART 基对象；公共 API 要求已初始化，测试替身忽略。
 * @pre 生产路径要求 uart_h 已初始化且已注册回调；测试替身不检查这些条件。
 * @post 函数体为空，不调用 tx_complete_cb，也不修改对象或测试状态。
 * @warning 测试替身不能验证发送完成通知链；生产回调通常运行在 ISR，必须保持短小。
 * @note 本函数是测试替身，仅用于满足链接依赖。
 */
void Dev_UART_Base_OnTxComplete(uart_base_t *uart_h) {
  (void)uart_h;
}

/**
 * @brief UART 端口初始化测试替身。
 * @param uart_h 待初始化的端口对象；公共 API 要求非 NULL，测试替身不访问。
 * @param name 调试名称；公共 API 要求非 NULL，测试替身不保存。
 * @param instance 板级 UART 实例；公共 API 要求已启用，测试替身忽略。
 * @retval PLATFORM_STATUS_OK 测试替身固定返回成功，不初始化对象或注册回调。
 * @pre 公共 UART 层已完成实例和回调配置校验；测试替身自身不检查参数。
 * @post uart_h 内容保持不变，Platform 回调表不被修改，也不访问 UART 硬件。
 * @note 本函数是测试替身；生产后端可能返回 BUSY 或 INVALID_ARGUMENT。
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
 * @brief JustFloat 遥测初始化测试替身。
 * @param telemetry_h 待初始化的遥测对象；公共 API 要求非 NULL，测试替身不访问。
 * @param name 调试名称；公共 API 要求非 NULL，测试替身不保存。
 * @param uart_h 遥测使用的 UART 基对象；公共 API 要求已初始化，测试替身不访问。
 * @param channel_count 每帧通道数；公共 API 限制为 1..TELEMETRY_CHANNEL_CAPACITY，测试替身忽略。
 * @pre 公共遥测层已完成参数校验；生产路径要求 UART 无进行中的 TX。
 * @post 函数体为空，不初始化遥测对象、不注册 TX 完成回调，也不取得 UART 所有权。
 * @note 本函数是测试替身，仅用于满足链接依赖。
 */
void Dev_JustFloat_UART_Init(
  justfloat_uart_t *telemetry_h,
  const char *name,
  uart_base_t *uart_h,
  uint8_t channel_count
) {
  (void)telemetry_h;
  (void)name;
  (void)uart_h;
  (void)channel_count;
}

/**
 * @brief 遥测样本提交测试替身。
 * @param telemetry_h 遥测基对象；公共 API 要求已初始化，测试替身忽略。
 * @param sample 调用方只读样本；测试替身不读取或保存。
 * @retval DEV_TELEMETRY_OK 测试替身固定返回成功，不编码或发送任何样本。
 * @pre 公共遥测层已验证句柄和样本；测试替身自身不检查参数。
 * @post sample 保持不变，不调用协议 send，也不改变忙状态或帧计数。
 * @note 本函数是测试替身；生产后端可能返回 BUSY 或 ERROR。
 */
dev_telemetry_status_t Dev_Telemetry_Base_Send(
  telemetry_base_t *telemetry_h,
  const telemetry_sample_t *sample
) {
  (void)telemetry_h;
  (void)sample;
  return DEV_TELEMETRY_OK;
}

/**
 * @brief 遥测状态查询测试替身。
 * @param telemetry_h 遥测基对象；测试替身不访问该参数。
 * @param status 调用方输出状态；必须非 NULL，测试替身将全部字段写零。
 * @pre status 指向至少一个 telemetry_status_t 的可写存储。
 * @post *status 被整体清零，不反映忙碌标志、已发送帧、丢弃帧或错误帧。
 * @warning 传 NULL 会触发 assert 并终止测试进程。
 * @note 本函数是测试替身，不访问具体协议后端。
 */
void Dev_Telemetry_Base_GetStatus(
  telemetry_base_t *telemetry_h,
  telemetry_status_t *status
) {
  (void)telemetry_h;
  assert(status);
  *status = (telemetry_status_t){0};
}

/**
 * @brief 运行本文件定义的契约测试并返回进程状态。
 * @retval 0 命令组帧、拒绝路径和超长命令契约均通过。
 * @post 进程以成功状态退出，不保留动态资源或硬件句柄。
 * @warning 任一契约断言失败都会终止进程，因此失败时不会返回状态码。
 */
int main(void) {
  test_command_framing();
  test_rejected_and_invalid_commands();
  test_overflow_and_ring_wrap();
  return 0;
}
