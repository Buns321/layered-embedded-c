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
 * @param line 可修改的 NUL 结尾命令行，可包含 `\r`。
 * @param command 输出命令。
 * @retval true 语法正确。
 * @retval false 空行、命令未知、参数缺失、数值非法或尾随多余字段。
 * @note 支持 `SET <name> <float>` 和 `RESET EKF`，大小写不敏感。
 * @note 参数名语义由命令处理回调校验，本函数只校验长度和语法。
 */
bool App_Command_Parse(char *line, app_uart_command_t *command);

/** @} */

#endif //LAYERED_EMBEDDED_C_APP_COMMAND_PARSER_H
