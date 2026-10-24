/**
 * @file platform_adc.c
 * @brief STM32F1 公共 ADC 实现的 HAL 后端。
 * @ingroup platform
 */

#include "adc.h"
#include "platform_adc_backend.h"
#include "platform_hal_internal.h"
#include "stm32f1xx_hal.h"

static ADC_HandleTypeDef *platform_adc_to_hal(platform_adc_instance_t adc) {
  switch (adc) {
#if PLATFORM_ENABLE_ADC1
    case PLATFORM_ADC_1: return &hadc1;
#endif
#if PLATFORM_ENABLE_ADC2
    case PLATFORM_ADC_2: return &hadc2;
#endif
    default: return NULL;
  }
}

static bool platform_hal_to_adc(
  ADC_HandleTypeDef *adc_h,
  platform_adc_instance_t *adc
) {
  if (adc_h == NULL || adc == NULL) {
    return false;
  }
#if PLATFORM_ENABLE_ADC1
  if (adc_h == &hadc1) {
    *adc = PLATFORM_ADC_1;
    return true;
  }
#endif
#if PLATFORM_ENABLE_ADC2
  if (adc_h == &hadc2) {
    *adc = PLATFORM_ADC_2;
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

bool platform_adc_backend_is_valid(platform_adc_instance_t adc) {
  return (uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT &&
         platform_adc_to_hal(adc) != NULL;
}

bool platform_adc_backend_supports_dma(platform_adc_instance_t adc) {
#if PLATFORM_ENABLE_ADC1
  return adc == PLATFORM_ADC_1;
#else
  (void)adc;
  return false;
#endif
}

platform_status_t platform_adc_backend_start(platform_adc_instance_t adc) {
  ADC_HandleTypeDef *adc_h = platform_adc_to_hal(adc);
  return adc_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_ADC_Start(adc_h));
}

platform_status_t platform_adc_backend_stop(platform_adc_instance_t adc) {
  ADC_HandleTypeDef *adc_h = platform_adc_to_hal(adc);
  return adc_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_ADC_Stop(adc_h));
}

platform_status_t platform_adc_backend_poll(
  platform_adc_instance_t adc,
  uint32_t timeout
) {
  ADC_HandleTypeDef *adc_h = platform_adc_to_hal(adc);
  return adc_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(
                       HAL_ADC_PollForConversion(adc_h, timeout)
                     );
}

platform_status_t platform_adc_backend_get_value(
  platform_adc_instance_t adc,
  uint32_t *value
) {
  ADC_HandleTypeDef *adc_h = platform_adc_to_hal(adc);
  if (adc_h == NULL || value == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  *value = HAL_ADC_GetValue(adc_h);
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_adc_backend_start_it(
  platform_adc_instance_t adc
) {
  ADC_HandleTypeDef *adc_h = platform_adc_to_hal(adc);
  return adc_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_ADC_Start_IT(adc_h));
}

platform_status_t platform_adc_backend_stop_it(
  platform_adc_instance_t adc
) {
  ADC_HandleTypeDef *adc_h = platform_adc_to_hal(adc);
  return adc_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_ADC_Stop_IT(adc_h));
}

platform_status_t platform_adc_backend_start_dma(
  platform_adc_instance_t adc,
  uint32_t *data,
  uint32_t length
) {
  ADC_HandleTypeDef *adc_h = platform_adc_to_hal(adc);
  return adc_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(
                       HAL_ADC_Start_DMA(adc_h, data, length)
                     );
}

platform_status_t platform_adc_backend_stop_dma(
  platform_adc_instance_t adc
) {
  ADC_HandleTypeDef *adc_h = platform_adc_to_hal(adc);
  return adc_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_ADC_Stop_DMA(adc_h));
}

uint32_t platform_adc_backend_get_error(platform_adc_instance_t adc) {
  ADC_HandleTypeDef *adc_h = platform_adc_to_hal(adc);
  return adc_h == NULL ? PLATFORM_ADC_ERROR_UNKNOWN
                   : platform_adc_map_error(HAL_ADC_GetError(adc_h));
}

static void platform_adc_dispatch_hal_event(
  ADC_HandleTypeDef *adc_h,
  platform_adc_event_type_t type
) {
  platform_adc_instance_t adc;
  if (platform_hal_to_adc(adc_h, &adc)) {
    platform_adc_dispatch_event(adc, type);
  }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *adc_h) {
  platform_adc_dispatch_hal_event(adc_h, PLATFORM_ADC_EVENT_CONV_COMPLETE);
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *adc_h) {
  platform_adc_dispatch_hal_event(adc_h, PLATFORM_ADC_EVENT_CONV_HALF_COMPLETE);
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *adc_h) {
  platform_adc_dispatch_hal_event(adc_h, PLATFORM_ADC_EVENT_ERROR);
}
