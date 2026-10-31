/**
 * @file system_text_contract_test.c
 * @brief System 通用文本切分和有限浮点解析契约测试。
 * @ingroup tests
 */

#include "system_text.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief 验证 ASCII 大小写不敏感比较。
 * @post 大小写不同但 ASCII 字母等价的文本相等，空字符串相等，NULL 和不同文本不相等。
 * @note 不修改输入，不分配内存。
 */
static void test_equals_ignore_case(void) {
  assert(Sys_Text_EqualsIgnoreCase("Reset", "RESET"));
  assert(Sys_Text_EqualsIgnoreCase("", ""));
  assert(!Sys_Text_EqualsIgnoreCase("SET", "SETTING"));
  assert(!Sys_Text_EqualsIgnoreCase(NULL, "SET"));
  assert(!Sys_Text_EqualsIgnoreCase("SET", NULL));
}

/**
 * @brief 验证空白切分、CR 处理和 token 容量限制。
 * @post 连续空白被折叠，三个字段正确输出；仅空白或超过容量的输入被拒绝。
 * @note 输入文本会被原地修改。
 */
static void test_split_whitespace(void) {
  char line[] = " \tSET\r  Q_ANGLE\t1.25e-4  ";
  char *tokens[3];
  uint32_t token_count = 0u;

  assert(Sys_Text_SplitWhitespace(
    line,
    tokens,
    3u,
    &token_count
  ));
  assert(token_count == 3u);
  assert(strcmp(tokens[0], "SET") == 0);
  assert(strcmp(tokens[1], "Q_ANGLE") == 0);
  assert(strcmp(tokens[2], "1.25e-4") == 0);

  char blank[] = " \t\r";
  assert(!Sys_Text_SplitWhitespace(blank, tokens, 3u, &token_count));

  char extra[] = "SET A 1 extra";
  assert(!Sys_Text_SplitWhitespace(extra, tokens, 3u, &token_count));

  assert(!Sys_Text_SplitWhitespace(NULL, tokens, 3u, &token_count));
  assert(!Sys_Text_SplitWhitespace(line, NULL, 3u, &token_count));
  assert(!Sys_Text_SplitWhitespace(line, tokens, 0u, &token_count));
  assert(!Sys_Text_SplitWhitespace(line, tokens, 3u, NULL));
}

/**
 * @brief 验证严格有限 float 解析。
 * @post 合法十进制、小数和指数被接受；空、残缺、尾随垃圾和非有限结果被拒绝。
 * @note 失败时输出值保持不变。
 */
static void test_parse_finite_f32(void) {
  float value = 42.0f;
  assert(Sys_Text_ParseFiniteF32("1.25e-4", &value));
  assert(fabsf(value - 0.000125f) < 1e-9f);

  assert(Sys_Text_ParseFiniteF32("-0.5", &value));
  assert(fabsf(value + 0.5f) < 1e-6f);

  assert(Sys_Text_ParseFiniteF32("+2", &value));
  assert(fabsf(value - 2.0f) < 1e-6f);

  value = 42.0f;
  assert(!Sys_Text_ParseFiniteF32("", &value));
  assert(value == 42.0f);
  assert(!Sys_Text_ParseFiniteF32(".", &value));
  assert(!Sys_Text_ParseFiniteF32("1e", &value));
  assert(!Sys_Text_ParseFiniteF32("1x", &value));
  assert(!Sys_Text_ParseFiniteF32("nan", &value));
  assert(!Sys_Text_ParseFiniteF32("1e1000000", &value));
  assert(!Sys_Text_ParseFiniteF32(NULL, &value));
  assert(!Sys_Text_ParseFiniteF32("1", NULL));
}

/**
 * @brief 运行 System 文本契约测试。
 * @return 全部断言通过后返回 0。
 * @post 测试栈对象失效，不保留动态资源。
 * @note 任一断言失败会终止进程。
 */
int main(void) {
  test_equals_ignore_case();
  test_split_whitespace();
  test_parse_finite_f32();
  return 0;
}
