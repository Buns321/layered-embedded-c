//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file app_command_parser.c
 * @brief 速通任务一 ASCII 参数命令解析实现。
 * @ingroup app
 */

#include "app_command_parser.h"
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

/**
 * @brief 不区分大小写比较命令文本。
 * @param left 左侧 NUL 结尾文本；必须非 NULL，允许空字符串，调试构建以 assert 检查。
 * @param right 右侧 NUL 结尾文本；必须非 NULL，允许空字符串，调试构建以 assert 检查。
 * @retval true 两个文本长度相同，且 ASCII 字母忽略大小写后逐字符相等。
 * @retval false 任一位置字符不同，或两者长度不同。
 * @pre left 和 right 非 NULL，并保持有效直到函数返回。
 * @note 只读纯函数；仅折叠 ASCII `A..Z`/`a..z`，不分配内存，不修改输入。
 */
static bool app_command_textequals(const char *left, const char *right) {
  assert(left && right);

  while (*left != '\0' && *right != '\0') {
    char left_char = *left;
    char right_char = *right;
    if (left_char >= 'a' && left_char <= 'z') {
      left_char = (char)(left_char - 'a' + 'A');
    }
    if (right_char >= 'a' && right_char <= 'z') {
      right_char = (char)(right_char - 'a' + 'A');
    }
    if (left_char != right_char) {
      return false;
    }
    left++;
    right++;
  }

  return *left == '\0' && *right == '\0';
}

/**
 * @brief 解析有限浮点数并拒绝尾随垃圾字符。
 * @param text NUL 结尾数值文本；允许可选正负号、十进制小数和 `e`/`E` 指数，不允许首尾空白。
 * @param value 输出浮点数；仅在返回 true 时写入，失败时保持调用前的值不变。
 * @retval true 文本完整构成有限 float，且至少有一位尾数数字；若存在指数，则指数形式完整。
 * @retval false 文本为空、缺少数字、指数不完整、含尾随字符，或结果溢出为非有限值。
 * @pre text 和 value 非 NULL，并保持有效直到函数返回。
 * @post 成功时 *value 为符号、尾数及可选指数共同表示的有限值；指数绝对值按最多 100 处理。
 * @note 仅在本编译单元使用；不分配内存，不依赖 locale，可由任意任务或 ISR 调用。
 */
static bool app_command_parsefloat(const char *text, float *value) {
  assert(text && value);

  const char *cursor = text;
  bool negative = false;
  bool digits_seen = false;
  float result = 0.0f;
  float fraction_scale = 0.1f;

  if (*cursor == '+' || *cursor == '-') {
    negative = *cursor == '-';
    cursor++;
  }

  while (*cursor >= '0' && *cursor <= '9') {
    result = result * 10.0f + (float)(*cursor - '0');
    digits_seen = true;
    cursor++;
  }

  if (*cursor == '.') {
    cursor++;
    while (*cursor >= '0' && *cursor <= '9') {
      result += (float)(*cursor - '0') * fraction_scale;
      fraction_scale *= 0.1f;
      digits_seen = true;
      cursor++;
    }
  }

  if (!digits_seen) {
    return false;
  }

  if (*cursor == 'e' || *cursor == 'E') {
    cursor++;
    bool exponent_negative = false;
    int exponent = 0;
    bool exponent_digit_seen = false;

    if (*cursor == '+' || *cursor == '-') {
      exponent_negative = *cursor == '-';
      cursor++;
    }
    while (*cursor >= '0' && *cursor <= '9') {
      if (exponent < 100) {
        exponent = exponent * 10 + (*cursor - '0');
      }
      exponent_digit_seen = true;
      cursor++;
    }
    if (!exponent_digit_seen) {
      return false;
    }

    float factor = 1.0f;
    for (int index = 0; index < exponent && index < 100; index++) {
      factor *= 10.0f;
    }
    result = exponent_negative ? result / factor : result * factor;
  }

  if (*cursor != '\0' || !isfinite(result)) {
    return false;
  }

  *value = negative ? -result : result;
  return true;
}

/**
 * @brief 去除字符串首尾空白并返回有效范围。
 * @param text 指向可写文本指针的地址；*text 必须指向 NUL 结尾可写字符串。
 * @pre text 和 *text 非 NULL，字符串可写，调试构建以 assert 检查。
 * @post *text 前进到首个非空格/制表符/`\r` 字符；尾部同类空白被替换为 NUL。
 * @note 原地修改字符串但不会移动内容；仅在本编译单元使用，不分配内存，不处理 `\n`。
 */
static void app_command_trim(char **text) {
  assert(text && *text);

  while (**text == ' ' || **text == '\t' || **text == '\r') {
    (*text)++;
  }

  char *end = *text + strlen(*text);
  while (
    end > *text &&
    (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')
  ) {
    end--;
  }
  *end = '\0';
}

/**
 * @brief 按分隔符拆分命令并限制片段数量。
 * @param line 可写 NUL 结尾命令行；函数会原地插入分隔 NUL 并去除字段首尾空白。
 * @param first 输出首字段指针；总是指向 line 中的非空文本。
 * @param second 输出第二字段指针；字段不存在时写 NULL。
 * @param third 输出第三字段起始指针；字段不存在时写 NULL，存在时保留字段内部空格直到行尾。
 * @retval true line 去除空白后至少包含一个字段；输出指针已更新。
 * @retval false line 仅含空白；输出指针保持调用前的值不变。
 * @pre line、first、second、third 非 NULL，line 可写并保持有效直到函数返回。
 * @post 最多切出三个字段，不再继续按空格拆分第三个字段。
 * @note 原地修改命令行；仅在本编译单元使用，不分配内存。
 */
static bool app_command_split(
  char *line,
  char **first,
  char **second,
  char **third
) {
  assert(line && first && second && third);

  app_command_trim(&line);
  if (*line == '\0') {
    return false;
  }

  *first = line;
  *second = NULL;
  *third = NULL;

  char *cursor = line;
  for (uint8_t token = 0u; token < 2u; token++) {
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
      cursor++;
    }
    if (*cursor == '\0') {
      break;
    }
    *cursor++ = '\0';
    while (*cursor == ' ' || *cursor == '\t') {
      cursor++;
    }
    if (token == 0u) {
      *second = cursor;
    } else {
      *third = cursor;
    }
  }

  if (*second != NULL) {
    app_command_trim(second);
    if (**second == '\0') {
      *second = NULL;
    }
  }
  if (*third != NULL) {
    app_command_trim(third);
    if (**third == '\0') {
      *third = NULL;
    }
  }

  return true;
}

/**
 * @brief 解析一行命令。
 * @param line 可写的 NUL 结尾命令行；函数会原地切分并去除字段首尾空格、制表符和 `\r`。
 * @param command 输出命令；仅返回 true 时可读取，返回 false 时内容未定义，不得依赖其 kind/name/value。
 * @retval true 命令为 `SET <name> <finite-float>` 或 `RESET EKF`，且没有多余字段。
 * @retval false 空行、命令未知、字段缺失、参数名过长、数值非有限、尾随垃圾或多余字段。
 * @pre line 和 command 非 NULL，且均可写并保持有效直到函数返回。
 * @post 成功时 command->kind 被设置；SET 的 name 为最长 23 字符的 NUL 结尾文本，value 为有限浮点数。
 * @note 文本比较仅对 ASCII 字母忽略大小写；本函数只校验语法，不判断参数名是否属于 EKF。
 * @note 命令解析在 VofaTask 上下文执行，不分配内存，也不访问硬件。
 */
bool App_Command_Parse(char *line, app_uart_command_t *command) {
  if (line == NULL || command == NULL) {
    return false;
  }

  char *first;
  char *second;
  char *third;
  if (!app_command_split(line, &first, &second, &third)) {
    return false;
  }

  *command = (app_uart_command_t){0};
  if (
    app_command_textequals(first, "RESET") &&
    second != NULL &&
    third == NULL &&
    app_command_textequals(second, "EKF")
  ) {
    command->kind = APP_UART_COMMAND_RESET_EKF;
    return true;
  }

  if (
    app_command_textequals(first, "SET") &&
    second != NULL &&
    third != NULL &&
    strlen(second) < sizeof(command->name) &&
    app_command_parsefloat(third, &command->value)
  ) {
    command->kind = APP_UART_COMMAND_SET;
    (void)strcpy(command->name, second);
    return true;
  }

  return false;
}
