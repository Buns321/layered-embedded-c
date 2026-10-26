//
// 由 Buns_ 创建于 2026/10/26。
//

/**
 * @file joycon_uart.c
 * @brief JoyCon ASCII UART 命令接收设备实现。
 * @ingroup drivers_concrete
 */

#include "joycon_uart.h"
#include "system_critical.h"
#include "system_math.h"
#include <assert.h>
#include <string.h>

#define JOYCON_UART_FIELD_COUNT 5u
#define JOYCON_UART_VALUE_COUNT 4u
#define JOYCON_UART_VALUE_LIMIT 100.0f

static bool Dev_JoyCon_UART_GetInputOp(
  joycon_base_t *joycon_h,
  joycon_input_t *input
);
static void Dev_JoyCon_UART_GetStatusOp(
  joycon_base_t *joycon_h,
  joycon_status_t *status
);

static const joycon_ops_t joycon_ops_uart = {
  .get_input = Dev_JoyCon_UART_GetInputOp,
  .get_status = Dev_JoyCon_UART_GetStatusOp,
};

static bool joycon_char_is_space(char value) {
  return value == ' ' || value == '\t';
}

static void joycon_reset_frame(joycon_uart_t *joycon_h) {
  assert(joycon_h);

  joycon_h->frame_length = 0u;
  joycon_h->frame_overflow = false;
  joycon_h->frame_active = false;
}

static bool joycon_parse_value(
  const char *text,
  uint16_t length,
  float *value
) {
  assert(text && value);

  uint16_t start = 0u;
  uint16_t end = length;

  while (start < end && joycon_char_is_space(text[start])) {
    start++;
  }
  while (end > start && joycon_char_is_space(text[end - 1u])) {
    end--;
  }

  if (start == end) {
    return false;
  }

  bool negative = false;
  if (text[start] == '+' || text[start] == '-') {
    negative = text[start] == '-';
    start++;
  }

  if (start == end) {
    return false;
  }

  float magnitude = 0.0f;
  float fraction_scale = 0.1f;
  bool has_digit = false;
  bool has_decimal_point = false;

  for (uint16_t i = start; i < end; i++) {
    char current = text[i];

    if (current == '.') {
      if (has_decimal_point) {
        return false;
      }
      has_decimal_point = true;
      continue;
    }

    if (current < '0' || current > '9') {
      return false;
    }

    has_digit = true;
    float digit = (float)(current - '0');

    if (has_decimal_point) {
      magnitude += digit * fraction_scale;
      fraction_scale *= 0.1f;
    } else {
      magnitude = magnitude * 10.0f + digit;
    }

    if (!SYS_ISFINITE(magnitude)) {
      return false;
    }
  }

  if (!has_digit || magnitude > JOYCON_UART_VALUE_LIMIT) {
    return false;
  }

  *value = negative ? -magnitude : magnitude;
  return true;
}

static bool joycon_parse_frame(
  const joycon_uart_t *joycon_h,
  uint32_t now_ms,
  joycon_input_t *input
) {
  assert(joycon_h && input);

  float values[JOYCON_UART_VALUE_COUNT] = {0};
  uint16_t cursor = 0u;

  for (uint16_t field = 0u; field < JOYCON_UART_FIELD_COUNT; field++) {
    uint16_t field_start = cursor;
    while (
      cursor < joycon_h->frame_length &&
      joycon_h->frame_text[cursor] != ','
    ) {
      cursor++;
    }

    uint16_t field_length = (uint16_t)(cursor - field_start);

    if (field == 0u) {
      if (
        field_length != 1u ||
        (joycon_h->frame_text[field_start] != 'j' &&
         joycon_h->frame_text[field_start] != 'J')
      ) {
        return false;
      }
    } else if (!joycon_parse_value(
                 &joycon_h->frame_text[field_start],
                 field_length,
                 &values[field - 1u]
               )) {
      return false;
    }

    bool has_separator =
      cursor < joycon_h->frame_length &&
      joycon_h->frame_text[cursor] == ',';

    if (field + 1u < JOYCON_UART_FIELD_COUNT) {
      if (!has_separator) {
        return false;
      }
      cursor++;
    } else if (has_separator || cursor != joycon_h->frame_length) {
      return false;
    }
  }

  *input = (joycon_input_t){
    .left_x = values[0] / JOYCON_UART_VALUE_LIMIT,
    .left_y = values[1] / JOYCON_UART_VALUE_LIMIT,
    .right_x = values[2] / JOYCON_UART_VALUE_LIMIT,
    .right_y = values[3] / JOYCON_UART_VALUE_LIMIT,
    .time_ms = now_ms,
    .sequence = joycon_h->input.sequence + 1u,
  };

  return true;
}

static void joycon_publish_frame(
  joycon_uart_t *joycon_h,
  uint32_t now_ms
) {
  assert(joycon_h);

  if (joycon_h->frame_overflow) {
    joycon_h->invalid_frame_count++;
    return;
  }

  joycon_input_t input = {0};
  if (!joycon_parse_frame(joycon_h, now_ms, &input)) {
    joycon_h->invalid_frame_count++;
    return;
  }

  Sys_Critical_Enter();
  joycon_h->input = input;
  joycon_h->parsed_frame_count++;
  Sys_Critical_Exit();
}

static void joycon_rearm_receive(joycon_uart_t *joycon_h) {
  assert(joycon_h && joycon_h->uart_h);

  dev_uart_status_t status = Dev_UART_ReceiveIT(
    joycon_h->uart_h,
    &joycon_h->rx_byte,
    1u
  );

  if (status == DEV_UART_OK) {
    joycon_h->rx_rearm_pending = false;
    return;
  }

  joycon_h->rx_rearm_pending = true;
  joycon_h->rearm_error_count++;
}

static void joycon_rx_complete_cb(
  uart_base_t *uart_h,
  void *context
) {
  joycon_uart_t *joycon_h = (joycon_uart_t *)context;
  assert(joycon_h && uart_h == joycon_h->uart_h);
  (void)uart_h;

  joycon_h->rx_byte_count++;

  uint16_t next_write =
    (joycon_h->rx_write + 1u) & JOYCON_UART_RX_INDEX_MASK;

  if (next_write == joycon_h->rx_read) {
    joycon_h->rx_overflow_count++;
  } else {
    joycon_h->rx_buffer[joycon_h->rx_write] = joycon_h->rx_byte;
    joycon_h->rx_write = next_write;
  }

  joycon_rearm_receive(joycon_h);
}

static void joycon_uart_error_cb(
  uart_base_t *uart_h,
  void *context
) {
  joycon_uart_t *joycon_h = (joycon_uart_t *)context;
  assert(joycon_h && uart_h == joycon_h->uart_h);
  (void)uart_h;

  joycon_h->uart_error_count++;
  joycon_rearm_receive(joycon_h);
}

void Dev_JoyCon_UART_Init(
  joycon_uart_t *joycon_h,
  const char *name,
  uart_base_t *uart_h
) {
  assert(joycon_h && name && uart_h);

  (void)memset(joycon_h, 0, sizeof(*joycon_h));
  Dev_JoyCon_Base_Init(&joycon_h->base, name, &joycon_ops_uart);
  joycon_h->uart_h = uart_h;
}

dev_uart_status_t Dev_JoyCon_UART_Start(joycon_uart_t *joycon_h) {
  assert(joycon_h && joycon_h->uart_h && !joycon_h->is_started);

  dev_uart_callbacks_t callbacks = {
    .rx_complete_cb = joycon_rx_complete_cb,
    .err_cb = joycon_uart_error_cb,
    .context = joycon_h,
  };

  Dev_UART_RegisterCallback(joycon_h->uart_h, &callbacks);
  joycon_h->is_started = true;
  joycon_rearm_receive(joycon_h);

  if (joycon_h->rx_rearm_pending) {
    return DEV_UART_ERROR;
  }

  return DEV_UART_OK;
}

void Dev_JoyCon_UART_Process(
  joycon_uart_t *joycon_h,
  uint32_t now_ms
) {
  assert(joycon_h && joycon_h->is_started);

  if (joycon_h->rx_rearm_pending) {
    joycon_rearm_receive(joycon_h);
  }

  while (joycon_h->rx_read != joycon_h->rx_write) {
    uint8_t byte = joycon_h->rx_buffer[joycon_h->rx_read];
    joycon_h->rx_read =
      (joycon_h->rx_read + 1u) & JOYCON_UART_RX_INDEX_MASK;
    Dev_JoyCon_UART_FeedByte(joycon_h, byte, now_ms);
  }
}

void Dev_JoyCon_UART_FeedByte(
  joycon_uart_t *joycon_h,
  uint8_t byte,
  uint32_t now_ms
) {
  assert(joycon_h);

  if (!joycon_h->frame_active) {
    if (byte == (uint8_t)'[') {
      joycon_h->frame_active = true;
      joycon_h->frame_length = 0u;
      joycon_h->frame_overflow = false;
    }
    return;
  }

  if (byte == (uint8_t)'[') {
    if (joycon_h->frame_length != 0u || joycon_h->frame_overflow) {
      joycon_h->invalid_frame_count++;
    }
    joycon_h->frame_length = 0u;
    joycon_h->frame_overflow = false;
    return;
  }

  if (byte == (uint8_t)']') {
    joycon_publish_frame(joycon_h, now_ms);
    joycon_reset_frame(joycon_h);
    return;
  }

  if (byte == (uint8_t)'\r' || byte == (uint8_t)'\n') {
    return;
  }

  if (joycon_h->frame_length >= JOYCON_UART_FRAME_CAPACITY) {
    joycon_h->frame_overflow = true;
    return;
  }

  joycon_h->frame_text[joycon_h->frame_length] = (char)byte;
  joycon_h->frame_length++;
}

bool Dev_JoyCon_UART_GetInput(
  joycon_uart_t *joycon_h,
  joycon_input_t *input
) {
  assert(joycon_h && input);

  return Dev_JoyCon_Base_GetInput(&joycon_h->base, input);
}

static bool Dev_JoyCon_UART_GetInputOp(
  joycon_base_t *joycon_h,
  joycon_input_t *input
) {
  assert(joycon_h && input);

  joycon_uart_t *joycon_uart_h = (joycon_uart_t *)joycon_h;

  Sys_Critical_Enter();
  *input = joycon_uart_h->input;
  bool available = input->sequence != 0u;
  Sys_Critical_Exit();

  return available;
}

void Dev_JoyCon_UART_GetStatus(
  joycon_uart_t *joycon_h,
  joycon_status_t *status
) {
  assert(joycon_h && status);

  Dev_JoyCon_Base_GetStatus(&joycon_h->base, status);
}

static void Dev_JoyCon_UART_GetStatusOp(
  joycon_base_t *joycon_h,
  joycon_status_t *status
) {
  assert(joycon_h && status);

  joycon_uart_t *joycon_uart_h = (joycon_uart_t *)joycon_h;

  Sys_Critical_Enter();
  *status = (joycon_status_t){
    .is_started = joycon_uart_h->is_started,
    .received_bytes = joycon_uart_h->rx_byte_count,
    .parsed_frames = joycon_uart_h->parsed_frame_count,
    .invalid_frames = joycon_uart_h->invalid_frame_count,
    .rx_overflow_count = joycon_uart_h->rx_overflow_count,
    .uart_error_count = joycon_uart_h->uart_error_count,
    .rearm_error_count = joycon_uart_h->rearm_error_count,
    .last_frame_ms = joycon_uart_h->input.time_ms,
    .sequence = joycon_uart_h->input.sequence,
  };
  Sys_Critical_Exit();
}
