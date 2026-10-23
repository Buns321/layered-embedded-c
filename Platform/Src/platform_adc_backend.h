/**
 * @file platform_adc_backend.h
 * @brief Private STM32-family backend for the common ADC implementation.
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_ADC_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_ADC_BACKEND_H

#include "platform_adc.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

bool platform_adc_backend_is_valid(Platform_ADC_Instance adc);

bool platform_adc_backend_supports_dma(Platform_ADC_Instance adc);

Platform_StatusTypeDef platform_adc_backend_start(Platform_ADC_Instance adc);

Platform_StatusTypeDef platform_adc_backend_stop(Platform_ADC_Instance adc);

Platform_StatusTypeDef platform_adc_backend_poll(
  Platform_ADC_Instance adc,
  uint32_t timeout
);

Platform_StatusTypeDef platform_adc_backend_get_value(
  Platform_ADC_Instance adc,
  uint32_t *value
);

Platform_StatusTypeDef platform_adc_backend_start_it(
  Platform_ADC_Instance adc
);

Platform_StatusTypeDef platform_adc_backend_stop_it(
  Platform_ADC_Instance adc
);

Platform_StatusTypeDef platform_adc_backend_start_dma(
  Platform_ADC_Instance adc,
  uint32_t *data,
  uint32_t length
);

Platform_StatusTypeDef platform_adc_backend_stop_dma(
  Platform_ADC_Instance adc
);

uint32_t platform_adc_backend_get_error(Platform_ADC_Instance adc);

void platform_adc_dispatch_event(
  Platform_ADC_Instance adc,
  Platform_ADC_EventType type
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_ADC_BACKEND_H
