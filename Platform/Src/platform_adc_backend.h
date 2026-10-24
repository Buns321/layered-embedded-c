/**
 * @file platform_adc_backend.h
 * @brief STM32 系列公共 ADC 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_ADC_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_ADC_BACKEND_H

#include "platform_adc.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

bool platform_adc_backend_is_valid(platform_adc_instance_t adc);

bool platform_adc_backend_supports_dma(platform_adc_instance_t adc);

platform_status_t platform_adc_backend_start(platform_adc_instance_t adc);

platform_status_t platform_adc_backend_stop(platform_adc_instance_t adc);

platform_status_t platform_adc_backend_poll(
  platform_adc_instance_t adc,
  uint32_t timeout
);

platform_status_t platform_adc_backend_get_value(
  platform_adc_instance_t adc,
  uint32_t *value
);

platform_status_t platform_adc_backend_start_it(
  platform_adc_instance_t adc
);

platform_status_t platform_adc_backend_stop_it(
  platform_adc_instance_t adc
);

platform_status_t platform_adc_backend_start_dma(
  platform_adc_instance_t adc,
  uint32_t *data,
  uint32_t length
);

platform_status_t platform_adc_backend_stop_dma(
  platform_adc_instance_t adc
);

uint32_t platform_adc_backend_get_error(platform_adc_instance_t adc);

void platform_adc_dispatch_event(
  platform_adc_instance_t adc,
  platform_adc_event_type_t type
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_ADC_BACKEND_H
