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
 */
uint32_t Sys_Log_TestGetTimeMs(void) {
  return s_test_time_ms;
}

/**
 * @brief 记录测试日志后端接收的数据并模拟写入限制。
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
 */
static void test_log_sink_reset(test_log_sink_t *sink_h) {
  (void)memset(sink_h, 0, sizeof(*sink_h));
  Sys_Log_Base_Init(&sink_h->base, "test", &s_test_log_ops);
  sink_h->accepted_limit = TEST_SINK_CAPACITY;
}

/**
 * @brief 比较测试日志后端内容与预期文本。
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
 */
static int test_log_argument(void) {
  s_argument_evaluations++;
  return 7;
}

/**
 * @brief 验证日志就绪状态和后端切换。
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
 * @brief 运行本文件定义的契约测试并返回进程状态。
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
