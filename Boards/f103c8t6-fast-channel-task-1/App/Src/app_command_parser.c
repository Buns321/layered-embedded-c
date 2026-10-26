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
