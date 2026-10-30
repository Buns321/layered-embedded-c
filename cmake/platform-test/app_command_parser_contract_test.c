/**
 * @file app_command_parser_contract_test.c
 * @brief 速通任务一 ASCII 参数命令解析契约测试。
 * @ingroup tests
 */

#include "app_command_parser.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

/**
 * @brief 判断两个单精度浮点数是否在容差内相等。
 * @param left 比较左值；量纲应与 right 相同。
 * @param right 比较右值；量纲应与 left 相同。
 * @param tolerance 允许的绝对误差，量纲与输入相同；负值会使比较恒为 false。
 * @retval true 差值绝对值不大于 tolerance。
 * @retval false 差值超过容差，或任一输入为 NaN。
 * @post 只读取输入，不修改解析器或命令对象。
 * @note 辅助函数不校验参数，也不使用断言。
 */
static bool float_near(float left, float right, float tolerance) {
  return fabsf(left - right) <= tolerance;
}

/**
 * @brief 验证 SET 命令的浮点格式与未知参数名语法。
 * @post 断言科学计数法、CR 结尾、大小写不敏感和负数均可解析，
 *       并确认解析器不限制参数名是否属于 EKF。
 * @note 调用真实解析实现，会就地修改 line 缓冲区；
 *       任一断言失败会终止进程，测试不访问硬件。
 */
static void test_set_command(void) {
  char line[] = "SET Q_ANGLE 1.25e-4\r";
  app_uart_command_t command;
  assert(App_Command_Parse(line, &command));
  assert(command.kind == APP_UART_COMMAND_SET);
  assert(strcmp(command.name, "Q_ANGLE") == 0);
  assert(float_near(command.value, 1.25e-4f, 1e-9f));

  char unknown_name_line[] = "set CUSTOM_PARAM -2.5";
  assert(App_Command_Parse(unknown_name_line, &command));
  assert(command.kind == APP_UART_COMMAND_SET);
  assert(strcmp(command.name, "CUSTOM_PARAM") == 0);
  assert(float_near(command.value, -2.5f, 1e-6f));
}

/**
 * @brief 验证 RESET EKF 命令的大小写不敏感解析。
 * @post 断言 command.kind 被写为 APP_UART_COMMAND_RESET_EKF。
 * @note 调用真实解析实现，会就地修改 line 缓冲区；
 *       任一断言失败会终止进程，测试不涉及硬件。
 */
static void test_reset_command(void) {
  char line[] = "reset ekf";
  app_uart_command_t command;
  assert(App_Command_Parse(line, &command));
  assert(command.kind == APP_UART_COMMAND_RESET_EKF);
}

/**
 * @brief 验证空行、缺参、非法数值、额外字段和空指针均被拒绝。
 * @post 断言独占空白的输入、缺少数值、非数字数值、尾随字段、未知 RESET
 *       目标、数值尾随字符以及 NULL 行指针全部返回 false。
 * @note 本用例只断言解析失败，不要求 command 在失败后保持不变；
 *       任一断言失败会终止进程。
 */
static void test_invalid_commands(void) {
  app_uart_command_t command;
  char empty[] = "   ";
  char missing_value[] = "SET Q_ANGLE";
  char bad_value[] = "SET Q_ANGLE nope";
  char extra_token[] = "SET Q_ANGLE 0.1 extra";
  char bad_reset[] = "RESET IMU";
  char trailing_value[] = "SET Q_ANGLE 0.1x";

  assert(!App_Command_Parse(empty, &command));
  assert(!App_Command_Parse(missing_value, &command));
  assert(!App_Command_Parse(bad_value, &command));
  assert(!App_Command_Parse(extra_token, &command));
  assert(!App_Command_Parse(bad_reset, &command));
  assert(!App_Command_Parse(trailing_value, &command));
  assert(!App_Command_Parse(NULL, &command));
}

/**
 * @brief 运行本文件定义的契约测试并返回进程状态。
 * @retval 0 三个命令解析契约测试均通过。
 * @post 进程以成功状态退出，不保留动态资源或硬件句柄。
 * @warning 任一契约断言失败都会终止进程，因此失败时不会返回状态码。
 */
int main(void) {
  test_set_command();
  test_reset_command();
  test_invalid_commands();
  return 0;
}
