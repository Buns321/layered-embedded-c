#include "platform_dac_backend.h"
#include "platform_dac.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static uint32_t s_backend_error;
static uint32_t s_event_count;
static Platform_DAC_Event s_last_event;

bool platform_dac_backend_is_valid(Platform_DAC_Instance dac) {
  return dac == PLATFORM_DAC_1;
}

Platform_StatusTypeDef platform_dac_backend_set_value(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel,
  Platform_DAC_Alignment alignment,
  uint32_t value
) {
  (void)dac;
  (void)channel;
  (void)alignment;
  (void)value;
  return Platform_OK;
}

Platform_StatusTypeDef platform_dac_backend_start(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel
) {
  (void)dac;
  (void)channel;
  return Platform_OK;
}

Platform_StatusTypeDef platform_dac_backend_stop(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel
) {
  (void)dac;
  (void)channel;
  return Platform_OK;
}

Platform_StatusTypeDef platform_dac_backend_start_dma(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel,
  Platform_DAC_Alignment alignment,
  const uint32_t *data,
  uint32_t length
) {
  (void)dac;
  (void)channel;
  (void)alignment;
  (void)data;
  (void)length;
  return Platform_OK;
}

Platform_StatusTypeDef platform_dac_backend_stop_dma(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel
) {
  (void)dac;
  (void)channel;
  return Platform_OK;
}

uint32_t platform_dac_backend_get_error(Platform_DAC_Instance dac) {
  (void)dac;
  return s_backend_error;
}

static void dac_callback(
  Platform_DAC_Instance dac,
  const Platform_DAC_Event *event,
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
         ) == Platform_OK);
  assert(Platform_DAC_SetValue(
           PLATFORM_DAC_1,
           PLATFORM_DAC_CHANNEL_1,
           PLATFORM_DAC_ALIGN_8_BIT_RIGHT,
           0x100U
         ) == Platform_INVALID_ARGUMENT);
  assert((Platform_DAC_GetLastError(PLATFORM_DAC_1) &
          PLATFORM_COMMON_ERROR_PARAMETER) != 0U);

  const uint32_t samples[2] = {1U, 2U};
  assert(Platform_DAC_Start_DMA(
           PLATFORM_DAC_1,
           PLATFORM_DAC_CHANNEL_2,
           PLATFORM_DAC_ALIGN_12_BIT_RIGHT,
           samples,
           2U
         ) == Platform_OK);
  assert(Platform_DAC_Start_DMA(
           PLATFORM_DAC_1,
           PLATFORM_DAC_CHANNEL_2,
           PLATFORM_DAC_ALIGN_12_BIT_RIGHT,
           NULL,
           2U
         ) == Platform_INVALID_ARGUMENT);

  const platform_dac_callbacks_t callbacks = {
    .callback = dac_callback,
    .context = &s_event_count,
  };
  assert(Platform_DAC_RegisterCallbacks(PLATFORM_DAC_1, &callbacks) ==
         Platform_OK);
  assert(Platform_DAC_RegisterCallbacks(PLATFORM_DAC_1, &callbacks) ==
         Platform_BUSY);

  s_backend_error = PLATFORM_DAC_ERROR_DMA_UNDERRUN;
  const Platform_DAC_Event event = {
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
