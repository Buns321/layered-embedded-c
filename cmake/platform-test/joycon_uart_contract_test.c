/**
 * @file joycon_uart_contract_test.c
 * @brief JoyCon UART ASCII 解析与快照契约的主机原生测试。
 * @ingroup tests
 */

#include "joycon_uart.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do {                                                \
  if (!(condition)) {                                                        \
    (void)fprintf(                                                           \
      stderr,                                                                \
      "%s:%d: check failed: %s\n",                                           \
      __FILE__,                                                              \
      __LINE__,                                                              \
      #condition                                                             \
    );                                                                       \
    return 1;                                                                \
  }                                                                          \
} while (0)

static uint8_t *s_rx_buffer;
static uint16_t s_rx_size;
static uint32_t s_receive_calls;
static uint32_t s_critical_nesting;
static dev_uart_status_t s_receive_status;

/**
 * @brief 增加测试临界嵌套计数。
 * @pre 无；本桩不检查调用上下文，也不要求先初始化其他对象。
 * @post s_critical_nesting 加一；不保存或修改 PRIMASK，不关闭中断，也不获取调度器互斥。
 * @note 必须与 Sys_Critical_Exit 成对调用，用于观察测试中临界区进入/退出次数。
 * @warning 这不是真实临界区，不提供互斥或中断保护；生产 System 实现才有对应语义。
 */
void Sys_Critical_Enter(void) {
  s_critical_nesting++;
}

/**
 * @brief 减少测试临界嵌套计数。
 * @pre 之前应调用过本文件的 Sys_Critical_Enter；本桩不验证配对关系。
 * @post 仅当 s_critical_nesting 大于零时减一，否则保持不变；不恢复 PRIMASK 或中断状态。
 * @note 必须与 Sys_Critical_Enter 成对调用，避免测试计数失衡。
 * @warning 这不是真实临界区退出，不提供中断恢复或调度保护。
 */
void Sys_Critical_Exit(void) {
  if (s_critical_nesting > 0u) {
    s_critical_nesting--;
  }
}

/**
 * @brief 记录 UART 接收挂接请求并返回注入状态。
 * @param uart_h UART 基对象；本替身忽略该参数，不校验对象类型。
 * @param data 下一次单字节接收目标地址；本替身只保存指针，不复制内容。
 * @param size 接收长度；本替身原样保存，通常为 1，本函数不检查范围。
 * @return s_receive_status 的当前值；测试注入 DEV_UART_OK 或 DEV_UART_ERROR。
 * @pre data 的生命周期应覆盖后续 feed_rx_byte 写入；本替身不检查 NULL。
 * @post s_rx_buffer 更新为 data，s_rx_size 更新为 size，s_receive_calls 加一。
 * @note 无论返回成功还是失败都会记录并计数；不启动真实 UART 中断或 DMA。
 */
static dev_uart_status_t fake_receive_it(
  uart_base_t *uart_h,
  uint8_t *data,
  uint16_t size
) {
  (void)uart_h;

  s_rx_buffer = data;
  s_rx_size = size;
  s_receive_calls++;
  return s_receive_status;
}

static const uart_ops_t s_fake_uart_ops = {
  .receive_it = fake_receive_it,
};

/**
 * @brief 重置 UART 测试桩记录。
 * @param uart_h 接收初始化的 UART 基对象；必须可写且生命周期覆盖后续测试。
 * @param joycon_h 接收初始化的 JoyCon 解析器对象；必须可写且不少于对象大小。
 * @retval 0 初始化、启动、单字节接收挂接和回调注册均符合预期。
 * @retval 1 启动失败、接收目标不是内部的 rx_byte/长度不是 1，或回调未安装。
 * @pre uart_h、joycon_h 指向独立且可写的对象，测试替身 ops 已链接。
 * @post 成功时临界计数归零、接收状态恢复为 DEV_UART_OK；启动挂接使
 *       s_receive_calls 从 0 变为 1，并挂接一个字节接收。失败时返回前可能只完成
 *       部分初始化，调用方必须停止后续步骤。
 * @note 使用返回码而非 assert 报告早期失败；不访问真实串口。
 */
static int fake_uart_reset(
  uart_base_t *uart_h,
  joycon_uart_t *joycon_h
) {
  s_rx_buffer = NULL;
  s_rx_size = 0u;
  s_receive_calls = 0u;
  s_critical_nesting = 0u;
  s_receive_status = DEV_UART_OK;

  Dev_UART_Base_Init(uart_h, "test_uart", &s_fake_uart_ops);
  Dev_JoyCon_UART_Init(joycon_h, "test_joycon", uart_h);
  if (Dev_JoyCon_UART_Start(joycon_h) != DEV_UART_OK) {
    return 1;
  }
  if (s_rx_buffer != &joycon_h->rx_byte || s_rx_size != 1u) {
    return 1;
  }
  if (uart_h->callbacks.rx_complete_cb == NULL ||
      uart_h->callbacks.err_cb == NULL) {
    return 1;
  }

  return 0;
}

/**
 * @brief 向 UART 接收路径注入单字节。
 * @param uart_h 已由 fake_uart_reset 初始化的 UART 基对象；其回调必须已安装。
 * @param joycon_h JoyCon 解析器对象；本函数不直接访问，由 UART 回调关联。
 * @param byte 注入的 8 位 UART 数据。
 * @pre fake_uart_reset 已成功执行，s_rx_buffer 非空并指向可写字节。
 * @post 目标字节被写入并同步调用 rx_complete_cb；解析器状态可能推进，
 *       回调重新挂接时 s_rx_buffer、s_rx_size 和 s_receive_calls 会再次更新。
 * @warning 当前触发是测试线程中的同步回调，不是真实 ISR；缓冲区失效会导致崩溃。
 */
static void feed_rx_byte(
  uart_base_t *uart_h,
  joycon_uart_t *joycon_h,
  uint8_t byte
) {
  (void)joycon_h;

  *s_rx_buffer = byte;
  uart_h->callbacks.rx_complete_cb(
    uart_h,
    uart_h->callbacks.context
  );
}

/**
 * @brief 向 UART 接收路径注入文本。
 * @param uart_h 已初始化的 UART 基对象。
 * @param joycon_h 已启动的 JoyCon 解析器对象。
 * @param text 以 NUL 结尾的 ASCII 帧文本；必须非空指针。
 * @pre fake_uart_reset 已成功执行；s_rx_buffer 对每次注入仍有效。
 * @post 按 strlen 结果逐字节调用 feed_rx_byte；不发送 NUL 终止符。
 * @note 文本可含 CR/LF，解析器按帧规则处理；空字符串只产生零次注入。
 * @warning 非 NUL 结尾的 text 会越界读取；真实 ISR 语义仍由同步回调模拟。
 */
static void feed_text(
  uart_base_t *uart_h,
  joycon_uart_t *joycon_h,
  const char *text
) {
  size_t length = strlen(text);
  for (size_t i = 0u; i < length; i++) {
    feed_rx_byte(uart_h, joycon_h, (uint8_t)text[i]);
  }
}

/**
 * @brief 验证有效帧和可选换行。
 * @retval 0 两帧均发布为预期比例值、时间戳和递增序号。
 * @retval 1 任一 CHECK 失败；失败条件及源码位置写入 stderr。
 * @pre 测试替身和 JoyCon ASCII 解析器已链接，静态接收状态可写。
 * @post 最终快照时间戳为 200 ms、序号为 2；临界嵌套计数回到 0。
 * @note 覆盖小写/大写帧头、CRLF 结尾、无换行结尾、负数和一位小数。
 * @warning CHECK 失败会提前返回 1；底层指针或状态断言触发时进程会终止。
 */
static int test_valid_frames_and_optional_newline(void) {
  uart_base_t uart = {0};
  joycon_uart_t joycon = {0};
  joycon_input_t input = {0};

  CHECK(fake_uart_reset(&uart, &joycon) == 0);

  feed_text(&uart, &joycon, "[j,100,-100,50,0]\r\n");
  Dev_JoyCon_UART_Process(&joycon, 100u);

  CHECK(Dev_JoyCon_Base_GetInput(&joycon.base, &input));
  CHECK(input.left_x == 1.0f);
  CHECK(input.left_y == -1.0f);
  CHECK(input.right_x == 0.5f);
  CHECK(input.right_y == 0.0f);
  CHECK(input.time_ms == 100u);
  CHECK(input.sequence == 1u);
  CHECK(s_critical_nesting == 0u);

  feed_text(&uart, &joycon, "[J,-25,12.5,-0.5,99]");
  Dev_JoyCon_UART_Process(&joycon, 200u);
  CHECK(Dev_JoyCon_Base_GetInput(&joycon.base, &input));
  CHECK(input.left_x == -0.25f);
  CHECK(input.left_y == 0.125f);
  CHECK(input.right_x == -0.005f);
  CHECK(input.right_y == 0.99f);
  CHECK(input.time_ms == 200u);
  CHECK(input.sequence == 2u);
  return 0;
}

/**
 * @brief 验证非法帧保留上次快照。
 * @retval 0 七种非法输入均被拒绝，上一份有效快照保持不变。
 * @retval 1 任一 CHECK 失败；失败条件及源码位置写入 stderr。
 * @pre fake_uart_reset 可成功挂接接收桩；解析器已启动。
 * @post 最终快照仍为序号 1、时间戳 10 ms；status.invalid_frames 为 7，
 *       status.parsed_frames 为 1。
 * @note 非法帧覆盖未知类型、越界、字段缺失、尾逗号、畸形小数和孤立负号。
 * @warning CHECK 失败会提前返回 1；底层断言触发时进程会终止。
 */
static int test_invalid_frames_keep_last_snapshot(void) {
  uart_base_t uart = {0};
  joycon_uart_t joycon = {0};
  joycon_input_t input = {0};
  joycon_status_t status = {0};
  const char *invalid_frames[] = {
    "[x,1,2,3,4]",
    "[j,101,0,0,0]",
    "[j,-101,0,0,0]",
    "[j,1,2,3]",
    "[j,1,2,3,4,]",
    "[j,1.2.3,0,0,0]",
    "[j,-,2,3,4]",
  };

  CHECK(fake_uart_reset(&uart, &joycon) == 0);

  feed_text(&uart, &joycon, "[j,10,20,30,40]");
  Dev_JoyCon_UART_Process(&joycon, 10u);
  CHECK(Dev_JoyCon_UART_GetInput(&joycon, &input));
  CHECK(input.sequence == 1u);

  for (uint32_t i = 0u; i < (uint32_t)(sizeof(invalid_frames) / sizeof(invalid_frames[0])); i++) {
    feed_text(&uart, &joycon, invalid_frames[i]);
    Dev_JoyCon_UART_Process(&joycon, 20u + i);
  }

  CHECK(Dev_JoyCon_UART_GetInput(&joycon, &input));
  CHECK(input.sequence == 1u);
  CHECK(input.time_ms == 10u);

  Dev_JoyCon_Base_GetStatus(&joycon.base, &status);
  CHECK(status.invalid_frames == 7u);
  CHECK(status.parsed_frames == 1u);
  return 0;
}

/**
 * @brief 验证分片帧与超长帧处理。
 * @retval 0 分片前缀被新帧头替换，合法完整帧发布正确数值，超长帧被拒绝。
 * @retval 1 任一 CHECK 失败；失败条件及源码位置写入 stderr。
 * @pre 接收桩保持每个单字节挂接，解析器状态可跨多次 feed 调用延续。
 * @post 最后有效快照保留第二帧的 0.03/0.04/0.05/0.06 比例值；
 *       超长输入后 invalid_frames 至少为 2、sequence 仍为 1。
 * @note 超长帧由 JOYCON_UART_FRAME_CAPACITY 限制；当前版本只统计拒绝次数。
 * @warning CHECK 失败会提前返回 1；底层断言触发时进程会终止。
 */
static int test_partial_and_overlong_frames(void) {
  uart_base_t uart = {0};
  joycon_uart_t joycon = {0};
  joycon_input_t input = {0};
  joycon_status_t status = {0};

  CHECK(fake_uart_reset(&uart, &joycon) == 0);

  feed_text(&uart, &joycon, "[j,1,2");
  feed_text(&uart, &joycon, "[j,3,4,5,6]");
  Dev_JoyCon_UART_Process(&joycon, 30u);

  CHECK(Dev_JoyCon_UART_GetInput(&joycon, &input));
  CHECK(input.left_x == 0.03f);
  CHECK(input.left_y == 0.04f);
  CHECK(input.right_x == 0.05f);
  CHECK(input.right_y == 0.06f);

  feed_text(&uart, &joycon, "[j,");
  for (uint32_t i = 0u; i < JOYCON_UART_FRAME_CAPACITY; i++) {
    feed_rx_byte(&uart, &joycon, (uint8_t)'0');
  }
  feed_text(&uart, &joycon, "]");
  Dev_JoyCon_UART_Process(&joycon, 40u);

  Dev_JoyCon_UART_GetStatus(&joycon, &status);
  CHECK(status.invalid_frames >= 2u);
  CHECK(status.sequence == 1u);
  return 0;
}

/**
 * @brief 验证 UART 错误后重新挂接接收。
 * @retval 0 错误回调使接收调用增加一次，且状态记录一次 UART 错误。
 * @retval 1 任一 CHECK 失败；失败条件及源码位置写入 stderr。
 * @pre fake_uart_reset 已成功，err_cb 和 context 均已注册。
 * @post s_receive_calls 比调用前多 1，status.uart_error_count 为 1。
 * @note 错误回调在测试线程中同步触发，不在真实中断上下文执行。
 * @warning CHECK 失败会提前返回 1；底层断言触发时进程会终止。
 */
static int test_uart_error_and_rearm(void) {
  uart_base_t uart = {0};
  joycon_uart_t joycon = {0};
  joycon_status_t status = {0};
  uint32_t receive_calls_before = 0u;

  CHECK(fake_uart_reset(&uart, &joycon) == 0);
  receive_calls_before = s_receive_calls;

  uart.callbacks.err_cb(&uart, uart.callbacks.context);
  CHECK(s_receive_calls == receive_calls_before + 1u);

  Dev_JoyCon_UART_GetStatus(&joycon, &status);
  CHECK(status.uart_error_count == 1u);
  return 0;
}

/**
 * @brief 验证启动失败后可恢复。
 * @retval 0 启动错误被保留为可恢复状态，后续 process 成功清除重挂待处理标志。
 * @retval 1 任一 CHECK 失败；失败条件及源码位置写入 stderr。
 * @pre UART 接收桩可被设置为 DEV_UART_ERROR，随后可恢复为 DEV_UART_OK。
 * @post 启动失败后 is_started 为 true、rx_rearm_pending 为 true、
 *       rearm_error_count 为 1；恢复处理后 rx_rearm_pending 为 false，错误计数仍为 1。
 * @note 该行为验证启动失败不会迫使上层重建对象；解析器仍未收到有效帧。
 * @warning CHECK 失败会提前返回 1；底层断言触发时进程会终止。
 */
static int test_start_failure_is_recoverable(void) {
  uart_base_t uart = {0};
  joycon_uart_t joycon = {0};
  joycon_status_t status = {0};

  Dev_UART_Base_Init(&uart, "test_uart", &s_fake_uart_ops);
  Dev_JoyCon_UART_Init(&joycon, "test_joycon", &uart);
  s_receive_status = DEV_UART_ERROR;

  CHECK(Dev_JoyCon_UART_Start(&joycon) == DEV_UART_ERROR);
  CHECK(joycon.rx_rearm_pending);

  Dev_JoyCon_UART_GetStatus(&joycon, &status);
  CHECK(status.is_started);
  CHECK(status.rearm_error_count == 1u);

  s_receive_status = DEV_UART_OK;
  Dev_JoyCon_UART_Process(&joycon, 50u);

  CHECK(!joycon.rx_rearm_pending);
  Dev_JoyCon_UART_GetStatus(&joycon, &status);
  CHECK(status.rearm_error_count == 1u);
  return 0;
}

/**
 * @brief 运行本文件定义的契约测试并返回进程状态。
 * @retval 0 五个 JoyCon UART 用例全部通过，并向 stdout 输出成功信息。
 * @retval 1 首个失败用例返回非零；后续用例不再执行。
 * @pre JoyCon 解析器、测试用 UART ops 和接收桩已链接。
 * @post 成功时标准输出包含固定通过信息；无动态内存或真实串口资源。
 * @warning 底层断言失败可能直接终止进程，而不是返回 1。
 */
int main(void) {
  if (test_valid_frames_and_optional_newline() != 0) {
    return 1;
  }
  if (test_invalid_frames_keep_last_snapshot() != 0) {
    return 1;
  }
  if (test_partial_and_overlong_frames() != 0) {
    return 1;
  }
  if (test_uart_error_and_rearm() != 0) {
    return 1;
  }
  if (test_start_failure_is_recoverable() != 0) {
    return 1;
  }

  (void)puts("joycon uart contract tests passed");
  return 0;
}
