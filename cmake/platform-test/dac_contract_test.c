/**
 * @file dac_contract_test.c
 * @brief DAC Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_dac_backend.h"
#include "platform_dac.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static uint32_t s_backend_error;
static uint32_t s_event_count;
static platform_dac_event_t s_last_event;

bool platform_dac_backend_is_valid(platform_dac_instance_t dac) {
  return dac == PLATFORM_DAC_1;
}

platform_status_t platform_dac_backend_set_value(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel,
  platform_dac_alignment_t alignment,
  uint32_t value
) {
  (void)dac;
  (void)channel;
  (void)alignment;
  (void)value;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_dac_backend_start(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
) {
  (void)dac;
  (void)channel;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_dac_backend_stop(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
) {
  (void)dac;
  (void)channel;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_dac_backend_start_dma(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel,
  platform_dac_alignment_t alignment,
  const uint32_t *data,
  uint32_t length
) {
  (void)dac;
  (void)channel;
  (void)alignment;
  (void)data;
  (void)length;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_dac_backend_stop_dma(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
) {
  (void)dac;
  (void)channel;
  return PLATFORM_STATUS_OK;
}

uint32_t platform_dac_backend_get_error(platform_dac_instance_t dac) {
  (void)dac;
  return s_backend_error;
}

static void dac_callback(
  platform_dac_instance_t dac,
  const platform_dac_event_t *event,
  void *context
) {
  assert(dac == PLATFORM_DAC_1);
  assert(event != NULL);
  assert(context == &s_event_count);
  s_event_count++;
  s_last_event = *event;
}

static void test_dac_contract(void) {
  s_backend_error = PLATFORM_DAC_ERROR_NONE;
  s_event_count = 0U;

  assert(Platform_DAC_SetValue(
           PLATFORM_DAC_1,
           PLATFORM_DAC_CHANNEL_1,
           PLATFORM_DAC_ALIGN_8_BIT_RIGHT,
           0xFFU
         ) == PLATFORM_STATUS_OK);
  assert(Platform_DAC_SetValue(
           PLATFORM_DAC_1,
           PLATFORM_DAC_CHANNEL_1,
           PLATFORM_DAC_ALIGN_8_BIT_RIGHT,
           0x100U
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_DAC_GetLastError(PLATFORM_DAC_1) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);

  const uint32_t samples[2] = {1U, 2U};
  assert(Platform_DAC_StartDMA(
           PLATFORM_DAC_1,
           PLATFORM_DAC_CHANNEL_2,
           PLATFORM_DAC_ALIGN_12_BIT_RIGHT,
           samples,
           2U
         ) == PLATFORM_STATUS_OK);
  assert(Platform_DAC_StartDMA(
           PLATFORM_DAC_1,
           PLATFORM_DAC_CHANNEL_2,
           PLATFORM_DAC_ALIGN_12_BIT_RIGHT,
           NULL,
           2U
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);

  const platform_dac_callback_binding_t callbacks = {
    .callback = dac_callback,
    .context = &s_event_count,
  };
  assert(Platform_DAC_RegisterCallback(PLATFORM_DAC_1, &callbacks) ==
         PLATFORM_STATUS_OK);
  assert(Platform_DAC_RegisterCallback(PLATFORM_DAC_1, &callbacks) ==
         PLATFORM_STATUS_BUSY);

  s_backend_error = PLATFORM_DAC_ERROR_DMA_UNDERRUN;
  const platform_dac_event_t event = {
    .type = PLATFORM_DAC_EVENT_UNDERRUN,
    .channel = PLATFORM_DAC_CHANNEL_1,
    .error = s_backend_error,
  };
  platform_dac_dispatch_event(PLATFORM_DAC_1, &event);
  assert(s_event_count == 1U);
  assert(s_last_event.type == PLATFORM_DAC_EVENT_UNDERRUN);
  assert(s_last_event.channel == PLATFORM_DAC_CHANNEL_1);
  assert(s_last_event.error == PLATFORM_DAC_ERROR_DMA_UNDERRUN);
}

int main(void) {
  test_dac_contract();
  return 0;
}
