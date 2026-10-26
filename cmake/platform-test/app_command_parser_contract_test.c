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

static bool float_near(float left, float right, float tolerance) {
  return fabsf(left - right) <= tolerance;
}

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

static void test_reset_command(void) {
  char line[] = "reset ekf";
  app_uart_command_t command;
  assert(App_Command_Parse(line, &command));
  assert(command.kind == APP_UART_COMMAND_RESET_EKF);
}

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

int main(void) {
  test_set_command();
  test_reset_command();
  test_invalid_commands();
  return 0;
}
