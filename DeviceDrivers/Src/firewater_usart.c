//
// 由 Buns_ 创建于 2026/10/21。
//

#include "firewater_usart.h"
#include "system_math.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

#define FIREWATER_MAX_PRECISION 6u
#define FIREWATER_MAX_SCALED_VALUE 1000000000.0f

static const uint32_t s_pow10[] = {
  1u, 10u, 100u, 1000u, 10000u, 100000u, 1000000u,
};

static void Dev_FireWater_USART_OnTxCplt(
  usart_base_t *usart,
  void *context
) {
  (void)usart;
  assert(context);

  firewater_usart_t *self = (firewater_usart_t *)context;
  self->sent_frames++;
  self->busy = false;
}

static bool Dev_FireWater_USART_AppendChar(
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

static bool Dev_FireWater_USART_AppendString(
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

static bool Dev_FireWater_USART_AppendUnsigned(
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

static bool Dev_FireWater_USART_AppendValue(
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
    if (!Dev_FireWater_USART_AppendChar(buffer, capacity, length, '-')) {
      return false;
    }
  }

  uint32_t integer_part = magnitude / scale;
  uint32_t decimal_part = magnitude % scale;

  if (!Dev_FireWater_USART_AppendUnsigned(
        buffer,
        capacity,
        length,
        integer_part)) {
    return false;
  }

  if (precision == 0u) {
    return true;
  }

  if (!Dev_FireWater_USART_AppendChar(buffer, capacity, length, '.')) {
    return false;
  }

  for (uint8_t divisor_index = precision; divisor_index > 0u; divisor_index--) {
    uint32_t divisor = s_pow10[divisor_index - 1u];
    char digit = (char)('0' + ((decimal_part / divisor) % 10u));

    if (!Dev_FireWater_USART_AppendChar(buffer, capacity, length, digit)) {
      return false;
    }
  }

  return true;
}

static bool Dev_FireWater_USART_Encode(
  firewater_usart_t *self,
  const telemetry_sample_t *sample,
  uint32_t *frame_size
) {
  uint32_t length = 0u;
  bool has_value = false;

  for (uint8_t channel = 0u; channel < self->channel_count; channel++) {
    uint32_t mask = (uint32_t)1u << channel;
    if ((sample->valid_mask & mask) == 0u) {
      continue;
    }

    if (has_value) {
      if (!Dev_FireWater_USART_AppendChar(
            self->tx_buffer,
            FIREWATER_USART_TX_CAPACITY,
            &length,
            ',')) {
        return false;
      }
    }

    if (
      !Dev_FireWater_USART_AppendString(
        self->tx_buffer,
        FIREWATER_USART_TX_CAPACITY,
        &length,
        self->channel_names[channel]) ||
      !Dev_FireWater_USART_AppendChar(
        self->tx_buffer,
        FIREWATER_USART_TX_CAPACITY,
        &length,
        ':') ||
      !Dev_FireWater_USART_AppendValue(
        self->tx_buffer,
        FIREWATER_USART_TX_CAPACITY,
        &length,
        sample->value[channel],
        self->precision)
    ) {
      return false;
    }

    has_value = true;
  }

  if (!has_value) {
    return false;
  }

  if (!Dev_FireWater_USART_AppendChar(
        self->tx_buffer,
        FIREWATER_USART_TX_CAPACITY,
        &length,
        '\n')) {
    return false;
  }

  *frame_size = length;
  return true;
}

static Dev_Telemetry_Status Dev_FireWater_USART_Send(
  telemetry_base_t *this,
  const telemetry_sample_t *sample
) {
  assert(this && sample);

  firewater_usart_t *self = (firewater_usart_t *)this;

  if (self->busy) {
    self->dropped_frames++;
    return DEV_TELEMETRY_BUSY;
  }

  uint32_t frame_size = 0u;
  if (!Dev_FireWater_USART_Encode(self, sample, &frame_size)) {
    self->error_count++;
    return DEV_TELEMETRY_ERROR;
  }

  self->busy = true;
  Dev_USART_Status status = Dev_USART_Transmit_DMA(
    self->usart,
    self->tx_buffer,
    (uint16_t)frame_size
  );

  if (status != DEV_USART_OK) {
    self->busy = false;

    if (status == DEV_USART_BUSY) {
      self->dropped_frames++;
      return DEV_TELEMETRY_BUSY;
    }

    self->error_count++;
    return DEV_TELEMETRY_ERROR;
  }

  return DEV_TELEMETRY_OK;
}

static void Dev_FireWater_USART_GetStatus(
  telemetry_base_t *this,
  telemetry_status_t *status
) {
  assert(this && status);

  firewater_usart_t *self = (firewater_usart_t *)this;
  *status = (telemetry_status_t){
    .is_busy = self->busy,
    .sent_frames = self->sent_frames,
    .dropped_frames = self->dropped_frames,
    .error_count = self->error_count,
  };
}

static const telemetry_ops_t telemetry_ops_firewater = {
  .send       = Dev_FireWater_USART_Send,
  .get_status = Dev_FireWater_USART_GetStatus,
};

void Dev_FireWater_USART_Init(
  firewater_usart_t *this,
  const char *name,
  usart_base_t *usart,
  const char *const channel_names[],
  uint8_t channel_count,
  uint8_t precision
) {
  assert(
    this &&
    name &&
    usart &&
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
    &this->base,
    name,
    &telemetry_ops_firewater
  );

  this->usart = usart;
  this->channel_names = channel_names;
  this->channel_count = channel_count;
  this->precision = precision;
  this->busy = false;
  this->sent_frames = 0u;
  this->dropped_frames = 0u;
  this->error_count = 0u;

  dev_usart_callbacks_t callbacks = {
    .tx_cplt_cb = Dev_FireWater_USART_OnTxCplt,
    .context = this,
  };
  Dev_USART_RegisterCallbacks(usart, &callbacks);
}
