//
// 由 Buns_ 创建于 2026/10/21。
//

#include "justfloat_usart.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static const uint8_t s_justfloat_tail[JUSTFLOAT_FRAME_TAIL_SIZE] = {
  0x00u, 0x00u, 0x80u, 0x7Fu,
};

static void Dev_JustFloat_USART_OnTxCplt(
  usart_base_t *usart,
  void *context
) {
  (void)usart;
  assert(context);

  justfloat_usart_t *self = (justfloat_usart_t *)context;
  self->sent_frames++;
  self->busy = false;
}

static void Dev_JustFloat_USART_StoreFloat(
  uint8_t *destination,
  float value
) {
  uint32_t bits;
  memcpy(&bits, &value, sizeof(bits));

  destination[0] = (uint8_t)(bits & 0xFFu);
  destination[1] = (uint8_t)((bits >> 8) & 0xFFu);
  destination[2] = (uint8_t)((bits >> 16) & 0xFFu);
  destination[3] = (uint8_t)((bits >> 24) & 0xFFu);
}

static Dev_Telemetry_Status Dev_JustFloat_USART_Send(
  telemetry_base_t *this,
  const telemetry_sample_t *sample
) {
  assert(this && sample);

  justfloat_usart_t *self = (justfloat_usart_t *)this;

  if (self->busy) {
    self->dropped_frames++;
    return DEV_TELEMETRY_BUSY;
  }

  if (
    self->channel_count == 0u ||
    self->channel_count > TELEMETRY_CHANNEL_CAPACITY
  ) {
    self->error_count++;
    return DEV_TELEMETRY_ERROR;
  }

  for (uint8_t channel = 0u; channel < self->channel_count; channel++) {
    uint32_t mask = (uint32_t)1u << channel;
    float value = (sample->valid_mask & mask) != 0u
      ? sample->value[channel]
      : NAN;

    Dev_JustFloat_USART_StoreFloat(
      &self->tx_buffer[(uint32_t)channel * 4u],
      value
    );
  }

  uint32_t payload_size = (uint32_t)self->channel_count * 4u;
  memcpy(
    &self->tx_buffer[payload_size],
    s_justfloat_tail,
    JUSTFLOAT_FRAME_TAIL_SIZE
  );

  uint32_t frame_size = payload_size + JUSTFLOAT_FRAME_TAIL_SIZE;

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

static void Dev_JustFloat_USART_GetStatus(
  telemetry_base_t *this,
  telemetry_status_t *status
) {
  assert(this && status);

  justfloat_usart_t *self = (justfloat_usart_t *)this;
  *status = (telemetry_status_t){
    .is_busy = self->busy,
    .sent_frames = self->sent_frames,
    .dropped_frames = self->dropped_frames,
    .error_count = self->error_count,
  };
}

static const telemetry_ops_t telemetry_ops_justfloat = {
  .send       = Dev_JustFloat_USART_Send,
  .get_status = Dev_JustFloat_USART_GetStatus,
};

void Dev_JustFloat_USART_Init(
  justfloat_usart_t *this,
  const char *name,
  usart_base_t *usart,
  uint8_t channel_count
) {
  assert(
    this &&
    name &&
    usart &&
    channel_count > 0u &&
    channel_count <= TELEMETRY_CHANNEL_CAPACITY
  );

  Dev_Telemetry_Base_Init(
    &this->base,
    name,
    &telemetry_ops_justfloat
  );

  this->usart = usart;
  this->channel_count = channel_count;
  this->busy = false;
  this->sent_frames = 0u;
  this->dropped_frames = 0u;
  this->error_count = 0u;

  dev_usart_callbacks_t callbacks = {
    .tx_cplt_cb = Dev_JustFloat_USART_OnTxCplt,
    .context = this,
  };
  Dev_USART_RegisterCallbacks(usart, &callbacks);
}
