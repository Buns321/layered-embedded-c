/**
 * @file system_log_contract_test.c
 * @brief System 日志门面与后端契约的主机原生测试。
 * @ingroup tests
 */

#include "system_log.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_SINK_CAPACITY 512u

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

typedef struct test_log_sink_s {
  sys_log_base_t base;
  uint8_t data[TEST_SINK_CAPACITY];
  uint32_t size;
  uint32_t accepted_limit;
  uint32_t write_calls;
  bool trigger_reentrant_log;
} test_log_sink_t;

static uint32_t s_test_time_ms;

/**
 * @brief 为主机日志测试提供可控的毫秒时间源。
 * @return 当前测试时间，单位 ms；取值为 uint32_t 全范围，按自然回绕处理。
 * @post 不修改时间值或其他测试状态。
 * @note 构建通过 SYS_LOG_EXTERNAL_TIME_SOURCE 注入该函数，本函数不访问硬件时基。
 */
uint32_t Sys_Log_TestGetTimeMs(void) {
  return s_test_time_ms;
}

/**
 * @brief 记录测试日志后端接收的数据并模拟写入限制。
 * @param log_h 日志后端基对象；必须位于 test_log_sink_t 首成员，测试桩会转换回具体类型。
 * @param data 待接收的只读字节缓冲区；size 大于 0 时必须可访问。
 * @param size 请求写入的字节数；函数接受其中受 accepted_limit 和剩余容量限制的部分。
 * @return 实际复制的字节数，范围 0..size；容量或限制为零时返回 0。
 * @pre log_h 必须指向已重置且 sink_h->size 不超过 TEST_SINK_CAPACITY 的 test_log_sink_t，data 必须覆盖 size 指定的只读字节；违反容量前提会使剩余空间计算下溢。
 * @post 数据追加到 sink_h->data 尾部且 sink_h->size 增加，write_calls 无条件加 1；一次性重入标志会被消费。
 * @note 设置 trigger_reentrant_log 后，本桩会在写入过程中发起嵌套 WARN，用于验证日志门面拒绝重入。
 * @warning 本桩不阻塞、不访问硬件；仅用于替换 sys_log_ops_t.write。
 */
static uint32_t test_log_sink_write(
  sys_log_base_t *log_h,
  const uint8_t *data,
  uint32_t size
) {
  test_log_sink_t *sink_h = (test_log_sink_t *)log_h;
  uint32_t accepted = size < sink_h->accepted_limit
    ? size
    : sink_h->accepted_limit;
  uint32_t available = TEST_SINK_CAPACITY - sink_h->size;

  if (accepted > available) {
    accepted = available;
  }

  if (accepted > 0u) {
    (void)memcpy(&sink_h->data[sink_h->size], data, accepted);
    sink_h->size += accepted;
  }
  sink_h->write_calls++;

  if (sink_h->trigger_reentrant_log) {
    sink_h->trigger_reentrant_log = false;
    SYS_LOG_WARN("NESTED", "blocked");
  }

  return accepted;
}

static const sys_log_ops_t s_test_log_ops = {
  .write = test_log_sink_write,
};

/**
 * @brief 重置测试日志后端状态。
 * @param sink_h 待重置的测试后端；不能为 NULL。
 * @pre sink_h 指向调用方拥有的可写 test_log_sink_t。
 * @post 数据、长度和计数均清零；base 重新绑定静态操作表，accepted_limit 恢复为 TEST_SINK_CAPACITY。
 * @note 本函数不使用断言，也不分配动态内存；调用方需保证指针有效。
 */
static void test_log_sink_reset(test_log_sink_t *sink_h) {
  (void)memset(sink_h, 0, sizeof(*sink_h));
  Sys_Log_Base_Init(&sink_h->base, "test", &s_test_log_ops);
  sink_h->accepted_limit = TEST_SINK_CAPACITY;
}

/**
 * @brief 比较测试日志后端内容与预期文本。
 * @param sink_h 已记录数据的测试后端；不能为 NULL。
 * @param expected 以 NUL 结尾的预期文本；函数用 strlen 计算比较长度。
 * @retval true 后端长度等于 expected 长度，且所有字节完全一致。
 * @retval false 长度不同或任一字节不同。
 * @pre sink_h 和 expected 必须在调用期间可访问，expected 必须是合法 C 字符串。
 * @post 不修改后端、预期文本或测试计数。
 * @note 比较按文本字节进行；sink_h->data 中后续未使用字节不参与比较。
 */
static bool test_log_sink_equals(
  const test_log_sink_t *sink_h,
  const char *expected
) {
  uint32_t expected_size = (uint32_t)strlen(expected);
  return
    sink_h->size == expected_size &&
    memcmp(sink_h->data, expected, expected_size) == 0;
}

static int s_argument_evaluations;

/**
 * @brief 记录惰性日志参数的求值次数并返回固定值。
 * @return 恒为 7，供日志格式化和惰性求值断言使用。
 * @post s_argument_evaluations 在每次调用时加 1。
 * @note 该副作用使测试能够确认被过滤的 SYS_LOG_DEBUG 参数表达式未执行。
 */
static int test_log_argument(void) {
  s_argument_evaluations++;
  return 7;
}

/**
 * @brief 验证日志就绪状态和后端切换。
 * @retval 0 所有 CHECK 通过。
 * @retval 1 任一 CHECK 失败；失败位置和表达式已输出到 stderr。
 * @post 成功路径最终关闭日志后端，并保留第一个后端 1 次、第二个后端 1 次写入观测值。
 * @note Sys_Log_Init(NULL)、Sys_Log_SetSink(NULL) 均关闭输出；替换后端后仅后续日志使用新后端。
 * @warning CHECK 失败通过 return 1 结束用例，不依赖 assert 终止进程。
 */
static int test_ready_and_sink_switching(void) {
  test_log_sink_t first_sink;
  test_log_sink_t second_sink;

  test_log_sink_reset(&first_sink);
  test_log_sink_reset(&second_sink);
  s_test_time_ms = 1u;

  Sys_Log_SetLevel(SYS_LOG_LEVEL_INFO);
  Sys_Log_Init(NULL);
  CHECK(!Sys_Log_IsReady());
  CHECK(!Sys_Log_IsEnabled(SYS_LOG_LEVEL_ERROR));

  SYS_LOG_ERROR("TEST", "disabled");
  CHECK(first_sink.write_calls == 0u);
  CHECK(second_sink.write_calls == 0u);

  Sys_Log_SetSink(&first_sink.base);
  CHECK(Sys_Log_IsReady());
  CHECK(Sys_Log_IsEnabled(SYS_LOG_LEVEL_ERROR));
  SYS_LOG_ERROR("TEST", "first");
  CHECK(first_sink.write_calls == 1u);

  Sys_Log_SetSink(&second_sink.base);
  CHECK(Sys_Log_IsReady());
  SYS_LOG_ERROR("TEST", "second");
  CHECK(first_sink.write_calls == 1u);
  CHECK(second_sink.write_calls == 1u);

  Sys_Log_SetSink(NULL);
  CHECK(!Sys_Log_IsReady());
  SYS_LOG_ERROR("TEST", "disabled again");
  CHECK(second_sink.write_calls == 1u);
  return 0;
}

/**
 * @brief 验证日志级别过滤与惰性参数。
 * @retval 0 所有 CHECK 通过。
 * @retval 1 任一 CHECK 失败；失败位置和表达式已输出到 stderr。
 * @post INFO 等级下 ERROR、WARN、INFO 各写一次而 DEBUG 被过滤；DEBUG 参数求值计数保持 0。
 * @note 日志宏先查询快速掩码，因此被过滤的参数表达式不会求值。
 * @warning CHECK 失败通过 return 1 结束用例，不依赖 assert 终止进程。
 */
static int test_level_filtering_and_lazy_arguments(void) {
  test_log_sink_t sink_h;

  test_log_sink_reset(&sink_h);
  s_test_time_ms = 2u;

  Sys_Log_Init(&sink_h.base);
  Sys_Log_SetLevel(SYS_LOG_LEVEL_INFO);
  CHECK(Sys_Log_GetLevel() == SYS_LOG_LEVEL_INFO);
  CHECK(Sys_Log_IsEnabled(SYS_LOG_LEVEL_ERROR));
  CHECK(Sys_Log_IsEnabled(SYS_LOG_LEVEL_WARN));
  CHECK(Sys_Log_IsEnabled(SYS_LOG_LEVEL_INFO));
  CHECK(!Sys_Log_IsEnabled(SYS_LOG_LEVEL_DEBUG));

  SYS_LOG_ERROR("TEST", "error");
  SYS_LOG_WARN("TEST", "warn");
  SYS_LOG_INFO("TEST", "info");
  SYS_LOG_DEBUG("TEST", "debug");
  CHECK(sink_h.write_calls == 3u);

  s_argument_evaluations = 0;
  SYS_LOG_DEBUG("TEST", "value=%d", test_log_argument());
  CHECK(s_argument_evaluations == 0);

  return 0;
}

/**
 * @brief 验证日志级别校验。
 * @retval 0 所有 CHECK 通过。
 * @retval 1 任一 CHECK 失败；失败位置和表达式已输出到 stderr。
 * @post 非法等级 99 被归一化为 SYS_LOG_LEVEL_NONE；用例末尾有效等级恢复为 DEBUG。
 * @note 后端存在时 NONE 仍使 Sys_Log_IsReady 返回 true，但所有等级输出均被禁用。
 * @warning CHECK 失败通过 return 1 结束用例，不依赖 assert 终止进程。
 */
static int test_level_validation(void) {
  test_log_sink_t sink_h;

  test_log_sink_reset(&sink_h);
  Sys_Log_Init(&sink_h.base);

  Sys_Log_SetLevel(SYS_LOG_LEVEL_NONE);
  CHECK(Sys_Log_GetLevel() == SYS_LOG_LEVEL_NONE);
  CHECK(Sys_Log_IsReady());
  CHECK(!Sys_Log_IsEnabled(SYS_LOG_LEVEL_ERROR));

  Sys_Log_SetLevel((sys_log_level_t)99);
  CHECK(Sys_Log_GetLevel() == SYS_LOG_LEVEL_NONE);
  CHECK(!Sys_Log_IsEnabled(SYS_LOG_LEVEL_ERROR));

  Sys_Log_SetLevel(SYS_LOG_LEVEL_DEBUG);
  CHECK(Sys_Log_IsEnabled(SYS_LOG_LEVEL_DEBUG));
  return 0;
}

/**
 * @brief 验证日志格式化与时间戳。
 * @retval 0 CHECK 通过，后端内容与预期格式完全一致。
 * @retval 1 CHECK 失败；失败位置和表达式已输出到 stderr。
 * @post 后端保存 "[1234][I][APP] value=42\r\n"，包含毫秒时间、等级、标签和 CRLF。
 * @note 时间戳来自测试时间源 s_test_time_ms，正文由 printf 风格格式参数生成。
 * @warning CHECK 失败通过 return 1 结束用例，不依赖 assert 终止进程。
 */
static int test_format_and_timestamp(void) {
  test_log_sink_t sink_h;

  test_log_sink_reset(&sink_h);
  s_test_time_ms = 1234u;

  Sys_Log_Init(&sink_h.base);
  Sys_Log_SetLevel(SYS_LOG_LEVEL_DEBUG);
  SYS_LOG_INFO("APP", "value=%d", 42);

  CHECK(test_log_sink_equals(&sink_h, "[1234][I][APP] value=42\r\n"));
  return 0;
}

/**
 * @brief 验证日志丢弃计数。
 * @retval 0 所有 CHECK 通过。
 * @retval 1 任一 CHECK 失败；失败位置和表达式已输出到 stderr。
 * @post 后端只接受前 5 byte，Sys_Log_GetDropBytes 报告整行剩余字节数；随后 Sys_Log_Init 将计数清零。
 * @note 丢弃计数按后端未接受的字节数累加，不把已接收部分计入丢弃。
 * @warning CHECK 失败通过 return 1 结束用例，不依赖 assert 终止进程。
 */
static int test_drop_accounting(void) {
  test_log_sink_t sink_h;
  const char *expected = "[10][E][TEST] x\r\n";

  test_log_sink_reset(&sink_h);
  s_test_time_ms = 10u;
  sink_h.accepted_limit = 5u;

  Sys_Log_Init(&sink_h.base);
  Sys_Log_SetLevel(SYS_LOG_LEVEL_ERROR);
  SYS_LOG_ERROR("TEST", "x");

  CHECK(sink_h.write_calls == 1u);
  CHECK(sink_h.size == 5u);
  CHECK(Sys_Log_GetDropBytes() == (uint32_t)strlen(expected) - 5u);

  Sys_Log_Init(&sink_h.base);
  CHECK(Sys_Log_GetDropBytes() == 0u);
  return 0;
}

/**
 * @brief 验证日志锁竞争计数。
 * @retval 0 所有 CHECK 通过。
 * @retval 1 任一 CHECK 失败；失败位置和表达式已输出到 stderr。
 * @post 后端仅记录外层日志一次，嵌套 WARN 因锁被占用被拒绝并使竞争计数为 1；随后 Init 将计数清零。
 * @note 测试后端在一次写入中触发重入，用于覆盖非阻塞日志门面的竞争失败路径。
 * @warning CHECK 失败通过 return 1 结束用例，不依赖 assert 终止进程。
 */
static int test_contention_accounting(void) {
  test_log_sink_t sink_h;

  test_log_sink_reset(&sink_h);
  s_test_time_ms = 20u;
  sink_h.trigger_reentrant_log = true;

  Sys_Log_Init(&sink_h.base);
  Sys_Log_SetLevel(SYS_LOG_LEVEL_INFO);
  SYS_LOG_INFO("OUTER", "message");

  CHECK(sink_h.write_calls == 1u);
  CHECK(Sys_Log_GetContentionCount() == 1u);

  Sys_Log_Init(&sink_h.base);
  CHECK(Sys_Log_GetContentionCount() == 0u);
  return 0;
}

/**
 * @brief 验证紧急原始日志路径。
 * @retval 0 所有 CHECK 通过。
 * @retval 1 任一 CHECK 失败；失败位置和表达式已输出到 stderr。
 * @post 有效载荷按原字节和原长度写入一次；NULL 指针及零长度调用不增加写入次数。
 * @note Sys_Log_EmergencyRaw 绕过格式化、级别过滤和门面锁，直接调用当前后端。
 * @warning 紧急路径不参与普通写互斥，调用方必须避免与并发后端写入同时使用。
 */
static int test_emergency_raw(void) {
  test_log_sink_t sink_h;
  const uint8_t payload[] = {0x11u, 0x00u, 0xFEu, 0xFFu};

  test_log_sink_reset(&sink_h);
  Sys_Log_Init(&sink_h.base);
  Sys_Log_EmergencyRaw(payload, sizeof(payload));

  CHECK(sink_h.write_calls == 1u);
  CHECK(sink_h.size == sizeof(payload));
  CHECK(memcmp(sink_h.data, payload, sizeof(payload)) == 0);

  Sys_Log_EmergencyRaw(NULL, 1u);
  Sys_Log_EmergencyRaw(payload, 0u);
  CHECK(sink_h.write_calls == 1u);
  return 0;
}

/**
 * @brief 验证超长日志被截断。
 * @retval 0 所有 CHECK 通过。
 * @retval 1 任一 CHECK 失败；失败位置和表达式已输出到 stderr。
 * @post 后端收到固定 191 byte 的单行，末两字节保持为 CRLF。
 * @note 399 byte 正文明文超过 192 byte 行缓冲，格式化后按容量上限截断而不是丢弃整条日志。
 * @warning CHECK 失败通过 return 1 结束用例，不依赖 assert 终止进程。
 */
static int test_long_message_is_clamped(void) {
  test_log_sink_t sink_h;
  char message[400];

  test_log_sink_reset(&sink_h);
  s_test_time_ms = 30u;
  (void)memset(message, 'A', sizeof(message) - 1u);
  message[sizeof(message) - 1u] = '\0';

  Sys_Log_Init(&sink_h.base);
  Sys_Log_SetLevel(SYS_LOG_LEVEL_ERROR);
  SYS_LOG_ERROR("TEST", "%s", message);

  CHECK(sink_h.write_calls == 1u);
  CHECK(sink_h.size == 191u);
  CHECK(sink_h.data[sink_h.size - 2u] == '\r');
  CHECK(sink_h.data[sink_h.size - 1u] == '\n');
  return 0;
}

/**
 * @brief 验证非法日志级别被忽略。
 * @retval 0 后端未写入且丢弃、竞争计数均保持 0。
 * @retval 1 任一 CHECK 失败；失败位置和表达式已输出到 stderr。
 * @post 非法等级 99 的调用在加锁和后端写入前返回。
 * @note 非法等级既不产生丢弃字节，也不计入锁竞争；测试不验证返回值，因为该函数返回 void。
 * @warning CHECK 失败通过 return 1 结束用例，不依赖 assert 终止进程。
 */
static int test_invalid_level_is_ignored(void) {
  test_log_sink_t sink_h;

  test_log_sink_reset(&sink_h);
  Sys_Log_Init(&sink_h.base);
  Sys_Log_SetLevel(SYS_LOG_LEVEL_DEBUG);

  Sys_Log_Write((sys_log_level_t)99, "TEST", "ignored");
  CHECK(sink_h.write_calls == 0u);
  CHECK(Sys_Log_GetDropBytes() == 0u);
  CHECK(Sys_Log_GetContentionCount() == 0u);
  return 0;
}

/**
 * @brief 验证非法日志标签被忽略。
 * @retval 0 NULL 标签和超长标签均未触发后端写入。
 * @retval 1 任一 CHECK 失败；失败位置和表达式已输出到 stderr。
 * @post 两次非法标签调用后 write_calls 仍为 0。
 * @note NULL 标签被显式拒绝；199 byte 标签使格式化前缀超过行缓冲，因此日志在写入前被丢弃。
 * @warning CHECK 失败通过 return 1 结束用例，不依赖 assert 终止进程。
 */
static int test_invalid_tag_is_ignored(void) {
  test_log_sink_t sink_h;
  char long_tag[200];

  test_log_sink_reset(&sink_h);
  Sys_Log_Init(&sink_h.base);
  Sys_Log_SetLevel(SYS_LOG_LEVEL_DEBUG);

  Sys_Log_Write(SYS_LOG_LEVEL_ERROR, NULL, "ignored");
  CHECK(sink_h.write_calls == 0u);

  (void)memset(long_tag, 'T', sizeof(long_tag) - 1u);
  long_tag[sizeof(long_tag) - 1u] = '\0';
  Sys_Log_Write(SYS_LOG_LEVEL_ERROR, long_tag, "ignored");
  CHECK(sink_h.write_calls == 0u);
  return 0;
}

/**
 * @brief 依次运行 System 日志门面的全部契约用例。
 * @retval 0 所有子用例返回 0，并已向标准输出打印通过提示。
 * @retval 1 首个失败子用例返回 1；后续用例不再执行。
 * @post 成功路径完成全部日志、计数和竞争路径验证；最后一个用例保留其局部测试后端配置，进程随后退出。
 * @note 子用例通过 CHECK 报告文件、行号和失败表达式。
 * @warning 本测试运行在主机进程，不验证真实 UART/RTT 中断或任务调度时序。
 */
int main(void) {
  if (test_ready_and_sink_switching() != 0) {
    return 1;
  }
  if (test_level_filtering_and_lazy_arguments() != 0) {
    return 1;
  }
  if (test_level_validation() != 0) {
    return 1;
  }
  if (test_format_and_timestamp() != 0) {
    return 1;
  }
  if (test_drop_accounting() != 0) {
    return 1;
  }
  if (test_contention_accounting() != 0) {
    return 1;
  }
  if (test_emergency_raw() != 0) {
    return 1;
  }
  if (test_long_message_is_clamped() != 0) {
    return 1;
  }
  if (test_invalid_level_is_ignored() != 0) {
    return 1;
  }
  if (test_invalid_tag_is_ignored() != 0) {
    return 1;
  }

  (void)puts("system log contract tests passed");
  return 0;
}
