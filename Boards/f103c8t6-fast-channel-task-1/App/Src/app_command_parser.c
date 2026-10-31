//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file app_command_parser.c
 * @brief 速通任务一 ASCII 参数命令解析实现。
 * @ingroup app
 */

#include "app_command_parser.h"
#include "system_text.h"
#include <stddef.h>
#include <string.h>

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

  char *tokens[3];
  uint32_t token_count = 0u;
  if (!Sys_Text_SplitWhitespace(line, tokens, 3u, &token_count)) {
    return false;
  }

  *command = (app_uart_command_t){0};
  if (
    token_count == 2u &&
    Sys_Text_EqualsIgnoreCase(tokens[0], "RESET") &&
    Sys_Text_EqualsIgnoreCase(tokens[1], "EKF")
  ) {
    command->kind = APP_UART_COMMAND_RESET_EKF;
    return true;
  }

  if (
    token_count == 3u &&
    Sys_Text_EqualsIgnoreCase(tokens[0], "SET") &&
    strlen(tokens[1]) < sizeof(command->name) &&
    Sys_Text_ParseFiniteF32(tokens[2], &command->value)
  ) {
    command->kind = APP_UART_COMMAND_SET;
    (void)strcpy(command->name, tokens[1]);
    return true;
  }

  return false;
}
