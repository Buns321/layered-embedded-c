//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file app_command_parser.h
 * @brief 速通任务一 ASCII 参数命令解析。
 * @addtogroup app
 * @{
 * @note 解析器只处理语法，不判断参数名称是否属于 EKF。
 */

#ifndef LAYERED_EMBEDDED_C_APP_COMMAND_PARSER_H
#define LAYERED_EMBEDDED_C_APP_COMMAND_PARSER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 参数命令类型。
 */
typedef enum app_uart_command_kind_e {
  APP_UART_COMMAND_NONE = 0U,
  APP_UART_COMMAND_SET,
  APP_UART_COMMAND_RESET_EKF,
} app_uart_command_kind_t;

/**
 * @brief 一条已解析的命令。
 */
typedef struct app_uart_command_s {
  /** @brief 命令类型。 */
  app_uart_command_kind_t kind;
  /** @brief SET 参数名，最长 23 个 ASCII 字符。 */
  char name[24];
  /** @brief SET 数值；RESET EKF 时为零。 */
  float value;
} app_uart_command_t;

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
bool App_Command_Parse(char *line, app_uart_command_t *command);

/** @} */

#endif //LAYERED_EMBEDDED_C_APP_COMMAND_PARSER_H
