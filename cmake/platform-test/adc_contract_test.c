#include "platform_adc_backend.h"
#include "platform_adc.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static bool s_supports_dma;
static Platform_StatusTypeDef s_poll_status;
static uint32_t s_value;
static uint32_t s_backend_error;
static uint32_t s_event_count;

bool platform_adc_backend_is_valid(Platform_ADC_Instance adc) {
  return adc == PLATFORM_ADC_1;
}

bool platform_adc_backend_supports_dma(Platform_ADC_Instance adc) {
  (void)adc;
  return s_supports_dma;
}

Platform_StatusTypeDef platform_adc_backend_start(Platform_ADC_Instance adc) {
  (void)adc;
  return Platform_OK;
}

Platform_StatusTypeDef platform_adc_backend_stop(Platform_ADC_Instance adc) {
  (void)adc;
  return Platform_OK;
}

Platform_StatusTypeDef platform_adc_backend_poll(
  Platform_ADC_Instance adc,
  uint32_t timeout
) {
  (void)adc;
  (void)timeout;
  return s_poll_status;
}

Platform_StatusTypeDef platform_adc_backend_get_value(
  Platform_ADC_Instance adc,
  uint32_t *value
) {
  (void)adc;
  *value = s_value;
  return Platform_OK;
}

Platform_StatusTypeDef platform_adc_backend_start_it(
  Platform_ADC_Instance adc
) {
  (void)adc;
  return Platform_OK;
}

Platform_StatusTypeDef platform_adc_backend_stop_it(
  Platform_ADC_Instance adc
) {
  (void)adc;
  return Platform_OK;
}

Platform_StatusTypeDef platform_adc_backend_start_dma(
  Platform_ADC_Instance adc,
  uint32_t *data,
  uint32_t length
) {
  (void)adc;
  (void)data;
  (void)length;
  return Platform_OK;
}

Platform_StatusTypeDef platform_adc_backend_stop_dma(
  Platform_ADC_Instance adc
) {
  (void)adc;
  return Platform_OK;
}

uint32_t platform_adc_backend_get_error(Platform_ADC_Instance adc) {
  (void)adc;
  return s_backend_error;
}

static void adc_callback(
  Platform_ADC_Instance adc,
  const Platform_ADC_Event *event,
  void *context
) {
  assert(adc == PLATFORM_ADC_1);
  assert(event != NULL);
  assert(context == &s_event_count);
  s_event_count++;
}

static void test_adc_contract(void) {
  s_supports_dma = false;
  s_poll_status = Platform_OK;
  s_value = 0x5A5U;
  s_backend_error = PLATFORM_ADC_ERROR_NONE;
  s_event_count = 0U;

  uint32_t value = 0U;
  assert(Platform_ADC_Read(PLATFORM_ADC_1, &value, 10U) == Platform_OK);
  assert(value == s_value);
  assert(Platform_ADC_GetValue(PLATFORM_ADC_1, &value) == Platform_OK);
  assert(Platform_ADC_GetValue(PLATFORM_ADC_1, NULL) ==
         Platform_INVALID_ARGUMENT);

  uint32_t dma_data[2] = {0};
  assert(Platform_ADC_Start_DMA(PLATFORM_ADC_1, dma_data, 2U) ==
         Platform_UNSUPPORTED);
  assert((Platform_ADC_GetLastError(PLATFORM_ADC_1) &
          PLATFORM_COMMON_ERROR_UNSUPPORTED) != 0U);

  s_supports_dma = true;
  assert(Platform_ADC_Start_DMA(PLATFORM_ADC_1, dma_data, 2U) == Platform_OK);

  s_poll_status = Platform_TIMEOUT;
  assert(Platform_ADC_Read(PLATFORM_ADC_1, &value, 1U) == Platform_TIMEOUT);
  assert((Platform_ADC_GetLastError(PLATFORM_ADC_1) &
          PLATFORM_COMMON_ERROR_TIMEOUT) != 0U);

  const platform_adc_callbacks_t callbacks = {
    .callback = adc_callback,
    .context = &s_event_count,
  };
  assert(Platform_ADC_RegisterCallbacks(PLATFORM_ADC_1, &callbacks) ==
         Platform_OK);
  assert(Platform_ADC_RegisterCallbacks(PLATFORM_ADC_1, &callbacks) ==
         Platform_BUSY);

  s_backend_error = PLATFORM_ADC_ERROR_OVERRUN;
  platform_adc_dispatch_event(PLATFORM_ADC_1, PLATFORM_ADC_EVENT_ERROR);
  assert(s_event_count == 1U);
}

int main(void) {
  test_adc_contract();
  return 0;
}
