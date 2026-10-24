/**
 * @file adc_contract_test.c
 * @brief ADC Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_adc_backend.h"
#include "platform_adc.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static bool s_supports_dma;
static platform_status_t s_poll_status;
static uint32_t s_value;
static uint32_t s_backend_error;
static uint32_t s_event_count;

bool platform_adc_backend_is_valid(platform_adc_instance_t adc) {
  return adc == PLATFORM_ADC_1;
}

bool platform_adc_backend_supports_dma(platform_adc_instance_t adc) {
  (void)adc;
  return s_supports_dma;
}

platform_status_t platform_adc_backend_start(platform_adc_instance_t adc) {
  (void)adc;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_adc_backend_stop(platform_adc_instance_t adc) {
  (void)adc;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_adc_backend_poll(
  platform_adc_instance_t adc,
  uint32_t timeout
) {
  (void)adc;
  (void)timeout;
  return s_poll_status;
}

platform_status_t platform_adc_backend_get_value(
  platform_adc_instance_t adc,
  uint32_t *value
) {
  (void)adc;
  *value = s_value;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_adc_backend_start_it(
  platform_adc_instance_t adc
) {
  (void)adc;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_adc_backend_stop_it(
  platform_adc_instance_t adc
) {
  (void)adc;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_adc_backend_start_dma(
  platform_adc_instance_t adc,
  uint32_t *data,
  uint32_t length
) {
  (void)adc;
  (void)data;
  (void)length;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_adc_backend_stop_dma(
  platform_adc_instance_t adc
) {
  (void)adc;
  return PLATFORM_STATUS_OK;
}

uint32_t platform_adc_backend_get_error(platform_adc_instance_t adc) {
  (void)adc;
  return s_backend_error;
}

static void adc_callback(
  platform_adc_instance_t adc,
  const platform_adc_event_t *event,
  void *context
) {
  assert(adc == PLATFORM_ADC_1);
  assert(event != NULL);
  assert(context == &s_event_count);
  s_event_count++;
}

static void test_adc_contract(void) {
  s_supports_dma = false;
  s_poll_status = PLATFORM_STATUS_OK;
  s_value = 0x5A5U;
  s_backend_error = PLATFORM_ADC_ERROR_NONE;
  s_event_count = 0U;

  uint32_t value = 0U;
  assert(Platform_ADC_Read(PLATFORM_ADC_1, &value, 10U) == PLATFORM_STATUS_OK);
  assert(value == s_value);
  assert(Platform_ADC_GetValue(PLATFORM_ADC_1, &value) == PLATFORM_STATUS_OK);
  assert(Platform_ADC_GetValue(PLATFORM_ADC_1, NULL) ==
         PLATFORM_STATUS_INVALID_ARGUMENT);

  uint32_t dma_data[2] = {0};
  assert(Platform_ADC_StartDMA(PLATFORM_ADC_1, dma_data, 2U) ==
         PLATFORM_STATUS_UNSUPPORTED);
  assert((Platform_ADC_GetLastError(PLATFORM_ADC_1) &
          PLATFORM_ERROR_UNSUPPORTED) != 0U);

  s_supports_dma = true;
  assert(Platform_ADC_StartDMA(PLATFORM_ADC_1, dma_data, 2U) == PLATFORM_STATUS_OK);

  s_poll_status = PLATFORM_STATUS_TIMEOUT;
  assert(Platform_ADC_Read(PLATFORM_ADC_1, &value, 1U) == PLATFORM_STATUS_TIMEOUT);
  assert((Platform_ADC_GetLastError(PLATFORM_ADC_1) &
          PLATFORM_ERROR_TIMEOUT) != 0U);

  const platform_adc_callback_binding_t callbacks = {
    .callback = adc_callback,
    .context = &s_event_count,
  };
  assert(Platform_ADC_RegisterCallback(PLATFORM_ADC_1, &callbacks) ==
         PLATFORM_STATUS_OK);
  assert(Platform_ADC_RegisterCallback(PLATFORM_ADC_1, &callbacks) ==
         PLATFORM_STATUS_BUSY);

  s_backend_error = PLATFORM_ADC_ERROR_OVERRUN;
  platform_adc_dispatch_event(PLATFORM_ADC_1, PLATFORM_ADC_EVENT_ERROR);
  assert(s_event_count == 1U);
}

int main(void) {
  test_adc_contract();
  return 0;
}
