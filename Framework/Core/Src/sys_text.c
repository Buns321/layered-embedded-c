/**
 * @file sys_text.c
 * @brief 受限 ASCII 文本切分、比较与有限浮点解析实现。
 * @ingroup sys_core
 */

#include "sys_text.h"
#include "sys_math.h"
#include <stddef.h>

static bool sys_text_isspace(char value) { return value == ' ' || value == '\t' || value == '\r'; }

bool Sys_Text_EqualsIgnoreCase(const char *left, const char *right) {
  if (left == NULL || right == NULL) {
    return false;
  }

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

bool Sys_Text_SplitWhitespace(char *line, char **tokens, uint32_t token_capacity,
                              uint32_t *token_count) {
  if (line == NULL || tokens == NULL || token_capacity == 0U || token_count == NULL) {
    return false;
  }

  char *cursor = line;
  while (sys_text_isspace(*cursor)) {
    cursor++;
  }
  if (*cursor == '\0') {
    return false;
  }

  uint32_t count = 0U;
  while (*cursor != '\0') {
    if (count >= token_capacity) {
      return false;
    }

    tokens[count++] = cursor;
    while (*cursor != '\0' && !sys_text_isspace(*cursor)) {
      cursor++;
    }
    if (*cursor == '\0') {
      break;
    }

    *cursor++ = '\0';
    while (sys_text_isspace(*cursor)) {
      cursor++;
    }
  }

  *token_count = count;
  return count != 0U;
}

bool Sys_Text_ParseFiniteF32(const char *text, float *value) {
  if (text == NULL || value == NULL) {
    return false;
  }

  const char *cursor = text;
  bool negative = false;
  bool digits_seen = false;
  float result = 0.0f;
  float fraction_scale = 0.1f;

  if (*cursor == '+' || *cursor == '-') {
    negative = (*cursor == '-');
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
    uint32_t exponent = 0U;
    bool exponent_digit_seen = false;

    if (*cursor == '+' || *cursor == '-') {
      exponent_negative = (*cursor == '-');
      cursor++;
    }
    while (*cursor >= '0' && *cursor <= '9') {
      uint32_t digit = (uint32_t)(*cursor - '0');
      if (exponent < 100U) {
        exponent = exponent * 10U + digit;
        if (exponent > 100U) {
          exponent = 100U;
        }
      }
      exponent_digit_seen = true;
      cursor++;
    }
    if (!exponent_digit_seen) {
      return false;
    }

    float factor = 1.0f;
    for (uint32_t index = 0U; index < exponent; index++) {
      factor *= 10.0f;
    }
    result = exponent_negative ? (result / factor) : (result * factor);
  }

  if (*cursor != '\0' || !SYS_ISFINITE(result)) {
    return false;
  }

  *value = negative ? -result : result;
  return true;
}
