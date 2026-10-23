//
// 由 Buns_ 创建于 2026/10/21。
//

#include "firewater_uart.h"
#include "system_math.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

#define FIREWATER_MAX_PRECISION 6u
#define FIREWATER_MAX_SCALED_VALUE 1000000000.0f

static const uint32_t s_pow10[] = {
  1u, 10u, 100u, 1000u, 10000u, 100000u, 1000000u,
};

static void Dev_FireWater_UART_OnTxCplt(
  uart_base_t *uart_h,
  void *context
) {
  (void)uart_h;
  assert(context);

  firewater_uart_t *firewater_uart_h = (firewater_uart_t *)context;
  firewater_uart_h->sent_frames++;
  firewater_uart_h->busy = false;
}

static bool Dev_FireWater_UART_AppendChar(
  uint8_t *buffer,
  uint32_t capacity,
  uint32_t *length,
  char value
) {
  if (*length >= capacity) {
    return false;
  }

  buffer[*length] = (uint8_t)value;
  (*length)++;
  return true;
}

static bool Dev_FireWater_UART_AppendString(
  uint8_t *buffer,
  uint32_t capacity,
  uint32_t *length,
  const char *value
) {
  uint32_t value_length = (uint32_t)strlen(value);

  if (
    *length > capacity ||
    value_length > capacity - *length
  ) {
    return false;
  }

  memcpy(&buffer[*length], value, value_length);
  *length += value_length;
  return true;
}

static bool Dev_FireWater_UART_AppendUnsigned(
  uint8_t *buffer,
  uint32_t capacity,
  uint32_t *length,
  uint32_t value
) {
  char digits[10];
  uint32_t digit_count = 0u;

  do {
    digits[digit_count++] = (char)('0' + (value % 10u));
    value /= 10u;
  } while (value != 0u);

  if (
    *length > capacity ||
    digit_count > capacity - *length
  ) {
    return false;
  }

  while (digit_count > 0u) {
    buffer[(*length)++] = (uint8_t)digits[--digit_count];
  }

  return true;
}

static bool Dev_FireWater_UART_AppendValue(
  uint8_t *buffer,
  uint32_t capacity,
  uint32_t *length,
  float value,
  uint8_t precision
) {
  if (!SYS_ISFINITE(value) || precision > FIREWATER_MAX_PRECISION) {
    return false;
  }

  uint32_t scale = s_pow10[precision];
  float scaled_value = value * (float)scale;

  if (
    !SYS_ISFINITE(scaled_value) ||
    SYS_ABS(scaled_value) > FIREWATER_MAX_SCALED_VALUE
  ) {
    return false;
  }

  int32_t integer_scaled = scaled_value >= 0.0f
    ? (int32_t)(scaled_value + 0.5f)
    : (int32_t)(scaled_value - 0.5f);

  uint32_t magnitude = integer_scaled < 0
    ? (uint32_t)(-integer_scaled)
    : (uint32_t)integer_scaled;

  if (integer_scaled < 0) {
    if (!Dev_FireWater_UART_AppendChar(buffer, capacity, length, '-')) {
      return false;
    }
  }

  uint32_t integer_part = magnitude / scale;
  uint32_t decimal_part = magnitude % scale;

  if (!Dev_FireWater_UART_AppendUnsigned(
        buffer,
        capacity,
        length,
        integer_part)) {
    return false;
  }

  if (precision == 0u) {
    return true;
  }

  if (!Dev_FireWater_UART_AppendChar(buffer, capacity, length, '.')) {
    return false;
  }

  for (uint8_t divisor_index = precision; divisor_index > 0u; divisor_index--) {
    uint32_t divisor = s_pow10[divisor_index - 1u];
    char digit = (char)('0' + ((decimal_part / divisor) % 10u));

    if (!Dev_FireWater_UART_AppendChar(buffer, capacity, length, digit)) {
      return false;
    }
  }

  return true;
}

static bool Dev_FireWater_UART_Encode(
  firewater_uart_t *firewater_uart_h,
  const telemetry_sample_t *sample,
  uint32_t *frame_size
) {
  uint32_t length = 0u;
  bool has_value = false;

  for (uint8_t channel = 0u; channel < firewater_uart_h->channel_count; channel++) {
    uint32_t mask = (uint32_t)1u << channel;
    if ((sample->valid_mask & mask) == 0u) {
      continue;
    }

    if (has_value) {
      if (!Dev_FireWater_UART_AppendChar(
            firewater_uart_h->tx_buffer,
            FIREWATER_UART_TX_CAPACITY,
            &length,
            ',')) {
        return false;
      }
    }

    if (
      !Dev_FireWater_UART_AppendString(
        firewater_uart_h->tx_buffer,
        FIREWATER_UART_TX_CAPACITY,
        &length,
        firewater_uart_h->channel_names[channel]) ||
      !Dev_FireWater_UART_AppendChar(
        firewater_uart_h->tx_buffer,
        FIREWATER_UART_TX_CAPACITY,
        &length,
        ':') ||
      !Dev_FireWater_UART_AppendValue(
        firewater_uart_h->tx_buffer,
        FIREWATER_UART_TX_CAPACITY,
        &length,
        sample->value[channel],
        firewater_uart_h->precision)
    ) {
      return false;
    }

    has_value = true;
  }

  if (!has_value) {
    return false;
  }

  if (!Dev_FireWater_UART_AppendChar(
        firewater_uart_h->tx_buffer,
        FIREWATER_UART_TX_CAPACITY,
        &length,
        '\n')) {
    return false;
  }

  *frame_size = length;
  return true;
}

static dev_telemetry_status_t Dev_FireWater_UART_Send(
  telemetry_base_t *telemetry_h,
  const telemetry_sample_t *sample
) {
  assert(telemetry_h && sample);

  firewater_uart_t *firewater_uart_h = (firewater_uart_t *)telemetry_h;

  if (firewater_uart_h->busy) {
    firewater_uart_h->dropped_frames++;
    return DEV_TELEMETRY_BUSY;
  }

  uint32_t frame_size = 0u;
  if (!Dev_FireWater_UART_Encode(firewater_uart_h, sample, &frame_size)) {
    firewater_uart_h->error_count++;
    return DEV_TELEMETRY_ERROR;
  }

  firewater_uart_h->busy = true;
  dev_uart_status_t status = Dev_UART_TransmitDMA(
    firewater_uart_h->uart_h,
    firewater_uart_h->tx_buffer,
    (uint16_t)frame_size
  );

  if (status != DEV_UART_OK) {
    firewater_uart_h->busy = false;

    if (status == DEV_UART_BUSY) {
      firewater_uart_h->dropped_frames++;
      return DEV_TELEMETRY_BUSY;
    }

    firewater_uart_h->error_count++;
    return DEV_TELEMETRY_ERROR;
  }

  return DEV_TELEMETRY_OK;
}

static void Dev_FireWater_UART_GetStatus(
  telemetry_base_t *telemetry_h,
  telemetry_status_t *status
) {
  assert(telemetry_h && status);

  firewater_uart_t *firewater_uart_h = (firewater_uart_t *)telemetry_h;
  *status = (telemetry_status_t){
    .is_busy = firewater_uart_h->busy,
    .sent_frames = firewater_uart_h->sent_frames,
    .dropped_frames = firewater_uart_h->dropped_frames,
    .error_count = firewater_uart_h->error_count,
  };
}

static const telemetry_ops_t telemetry_ops_firewater = {
  .send       = Dev_FireWater_UART_Send,
  .get_status = Dev_FireWater_UART_GetStatus,
};

void Dev_FireWater_UART_Init(
  firewater_uart_t *telemetry_h,
  const char *name,
  uart_base_t *uart_h,
  const char *const channel_names[],
  uint8_t channel_count,
  uint8_t precision
) {
  assert(
    telemetry_h &&
    name &&
    uart_h &&
    channel_names &&
    channel_count > 0u &&
    channel_count <= TELEMETRY_CHANNEL_CAPACITY &&
    precision <= FIREWATER_MAX_PRECISION
  );

  for (uint8_t channel = 0u; channel < channel_count; channel++) {
    assert(
      channel_names[channel] &&
      channel_names[channel][0] != '\0' &&
      strchr(channel_names[channel], ',') == NULL &&
      strchr(channel_names[channel], ':') == NULL
    );
  }

  Dev_Telemetry_Base_Init(
    &telemetry_h->base,
    name,
    &telemetry_ops_firewater
  );

  telemetry_h->uart_h = uart_h;
  telemetry_h->channel_names = channel_names;
  telemetry_h->channel_count = channel_count;
  telemetry_h->precision = precision;
  telemetry_h->busy = false;
  telemetry_h->sent_frames = 0u;
  telemetry_h->dropped_frames = 0u;
  telemetry_h->error_count = 0u;

  dev_uart_callbacks_t callbacks = {
    .tx_complete_cb = Dev_FireWater_UART_OnTxCplt,
    .context = telemetry_h,
  };
  Dev_UART_RegisterCallback(uart_h, &callbacks);
}
