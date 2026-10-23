/**
 * @file platform_adc.c
 * @brief STM32F4 HAL backend for the common ADC implementation.
 */

#include "adc.h"
#include "platform_adc_backend.h"
#include "platform_hal_internal.h"
#include "stm32f4xx_hal.h"

static ADC_HandleTypeDef *platform_adc_to_hal(Platform_ADC_Instance adc) {
  switch (adc) {
#if PLATFORM_ENABLE_ADC1
    case PLATFORM_ADC_1: return &hadc1;
#endif
#if PLATFORM_ENABLE_ADC2
    case PLATFORM_ADC_2: return &hadc2;
#endif
#if PLATFORM_ENABLE_ADC3
    case PLATFORM_ADC_3: return &hadc3;
#endif
    default: return NULL;
  }
}

static bool platform_hal_to_adc(
  ADC_HandleTypeDef *hadc,
  Platform_ADC_Instance *adc
) {
  if (hadc == NULL || adc == NULL) {
    return false;
  }
#if PLATFORM_ENABLE_ADC1
  if (hadc == &hadc1) {
    *adc = PLATFORM_ADC_1;
    return true;
  }
#endif
#if PLATFORM_ENABLE_ADC2
  if (hadc == &hadc2) {
    *adc = PLATFORM_ADC_2;
    return true;
  }
#endif
#if PLATFORM_ENABLE_ADC3
  if (hadc == &hadc3) {
    *adc = PLATFORM_ADC_3;
    return true;
  }
#endif
  return false;
}

static uint32_t platform_adc_map_error(uint32_t hal_error) {
  uint32_t error = PLATFORM_ADC_ERROR_NONE;
  if ((hal_error & HAL_ADC_ERROR_INTERNAL) != 0U) {
    error |= PLATFORM_ADC_ERROR_INTERNAL;
  }
  if ((hal_error & HAL_ADC_ERROR_OVR) != 0U) {
    error |= PLATFORM_ADC_ERROR_OVERRUN;
  }
  if ((hal_error & HAL_ADC_ERROR_DMA) != 0U) {
    error |= PLATFORM_ADC_ERROR_DMA;
  }
  const uint32_t known_errors =
    HAL_ADC_ERROR_INTERNAL |
    HAL_ADC_ERROR_OVR |
    HAL_ADC_ERROR_DMA;
  if ((hal_error & ~known_errors) != 0U) {
    error |= PLATFORM_ADC_ERROR_UNKNOWN;
  }
  return error;
}

bool platform_adc_backend_is_valid(Platform_ADC_Instance adc) {
  return (uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT &&
         platform_adc_to_hal(adc) != NULL;
}

bool platform_adc_backend_supports_dma(Platform_ADC_Instance adc) {
  return platform_adc_backend_is_valid(adc);
}

Platform_StatusTypeDef platform_adc_backend_start(Platform_ADC_Instance adc) {
  ADC_HandleTypeDef *h = platform_adc_to_hal(adc);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_ADC_Start(h));
}

Platform_StatusTypeDef platform_adc_backend_stop(Platform_ADC_Instance adc) {
  ADC_HandleTypeDef *h = platform_adc_to_hal(adc);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_ADC_Stop(h));
}

Platform_StatusTypeDef platform_adc_backend_poll(
  Platform_ADC_Instance adc,
  uint32_t timeout
) {
  ADC_HandleTypeDef *h = platform_adc_to_hal(adc);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(
                       HAL_ADC_PollForConversion(h, timeout)
                     );
}

Platform_StatusTypeDef platform_adc_backend_get_value(
  Platform_ADC_Instance adc,
  uint32_t *value
) {
  ADC_HandleTypeDef *h = platform_adc_to_hal(adc);
  if (h == NULL || value == NULL) {
    return Platform_INVALID_ARGUMENT;
  }
  *value = HAL_ADC_GetValue(h);
  return Platform_OK;
}

Platform_StatusTypeDef platform_adc_backend_start_it(
  Platform_ADC_Instance adc
) {
  ADC_HandleTypeDef *h = platform_adc_to_hal(adc);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_ADC_Start_IT(h));
}

Platform_StatusTypeDef platform_adc_backend_stop_it(
  Platform_ADC_Instance adc
) {
  ADC_HandleTypeDef *h = platform_adc_to_hal(adc);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_ADC_Stop_IT(h));
}

Platform_StatusTypeDef platform_adc_backend_start_dma(
  Platform_ADC_Instance adc,
  uint32_t *data,
  uint32_t length
) {
  ADC_HandleTypeDef *h = platform_adc_to_hal(adc);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(
                       HAL_ADC_Start_DMA(h, data, length)
                     );
}

Platform_StatusTypeDef platform_adc_backend_stop_dma(
  Platform_ADC_Instance adc
) {
  ADC_HandleTypeDef *h = platform_adc_to_hal(adc);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_ADC_Stop_DMA(h));
}

uint32_t platform_adc_backend_get_error(Platform_ADC_Instance adc) {
  ADC_HandleTypeDef *h = platform_adc_to_hal(adc);
  return h == NULL ? PLATFORM_ADC_ERROR_UNKNOWN
                   : platform_adc_map_error(HAL_ADC_GetError(h));
}

static void platform_adc_dispatch_hal_event(
  ADC_HandleTypeDef *hadc,
  Platform_ADC_EventType type
) {
  Platform_ADC_Instance adc;
  if (platform_hal_to_adc(hadc, &adc)) {
    platform_adc_dispatch_event(adc, type);
  }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
  platform_adc_dispatch_hal_event(hadc, PLATFORM_ADC_EVENT_CONV_CPLT);
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
  platform_adc_dispatch_hal_event(hadc, PLATFORM_ADC_EVENT_CONV_HALF);
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc) {
  platform_adc_dispatch_hal_event(hadc, PLATFORM_ADC_EVENT_ERROR);
}
