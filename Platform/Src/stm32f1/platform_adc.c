//
// Created by Buns_ on 2026/10/23.
//

#include "platform_adc.h"
#include "platform_hal_internal.h"
#include "adc.h"
#include "stm32f1xx_hal.h"
#include <assert.h>
#include <stdbool.h>

static platform_adc_callbacks_t adc_cb[PLATFORM_ADC_INSTANCE_COUNT];
static bool adc_cb_registered[PLATFORM_ADC_INSTANCE_COUNT];

static ADC_HandleTypeDef *platform_adc_to_hal(Platform_ADC_Instance adc) {
  switch (adc) {
#if PLATFORM_ENABLE_ADC1
    case PLATFORM_ADC_1: return &hadc1;
#endif
#if PLATFORM_ENABLE_ADC2
    case PLATFORM_ADC_2: return &hadc2;
#endif
    default: assert(0); return NULL;
  }
}

static bool platform_hal_to_adc(
  ADC_HandleTypeDef *hadc,
  Platform_ADC_Instance *adc
) {
  assert(hadc != NULL && adc != NULL);
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

static bool platform_adc_supports_dma(Platform_ADC_Instance adc) {
#if PLATFORM_ENABLE_ADC1
  return adc == PLATFORM_ADC_1;
#else
  (void)adc;
  return false;
#endif
}

Platform_StatusTypeDef Platform_ADC_Start(Platform_ADC_Instance adc) {
  assert((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT);
  ADC_HandleTypeDef *hadc = platform_adc_to_hal(adc);
  assert(hadc != NULL);
  return platform_from_hal_status(HAL_ADC_Start(hadc));
}

Platform_StatusTypeDef Platform_ADC_Stop(Platform_ADC_Instance adc) {
  assert((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT);
  ADC_HandleTypeDef *hadc = platform_adc_to_hal(adc);
  assert(hadc != NULL);
  return platform_from_hal_status(HAL_ADC_Stop(hadc));
}

Platform_StatusTypeDef Platform_ADC_PollForConversion(
  Platform_ADC_Instance adc,
  uint32_t timeout
) {
  assert((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT);
  ADC_HandleTypeDef *hadc = platform_adc_to_hal(adc);
  assert(hadc != NULL);
  return platform_from_hal_status(HAL_ADC_PollForConversion(hadc, timeout));
}

uint32_t Platform_ADC_GetValue(Platform_ADC_Instance adc) {
  assert((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT);
  ADC_HandleTypeDef *hadc = platform_adc_to_hal(adc);
  assert(hadc != NULL);
  return HAL_ADC_GetValue(hadc);
}

Platform_StatusTypeDef Platform_ADC_Read(
  Platform_ADC_Instance adc,
  uint32_t *value,
  uint32_t timeout
) {
  assert(value != NULL);
  Platform_StatusTypeDef status = Platform_ADC_Start(adc);
  if (status != Platform_OK) {
    return status;
  }
  status = Platform_ADC_PollForConversion(adc, timeout);
  if (status == Platform_OK) {
    *value = Platform_ADC_GetValue(adc);
  }
  Platform_StatusTypeDef stop_status = Platform_ADC_Stop(adc);
  return status == Platform_OK ? stop_status : status;
}

Platform_StatusTypeDef Platform_ADC_Start_IT(Platform_ADC_Instance adc) {
  assert((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT);
  ADC_HandleTypeDef *hadc = platform_adc_to_hal(adc);
  assert(hadc != NULL);
  return platform_from_hal_status(HAL_ADC_Start_IT(hadc));
}

Platform_StatusTypeDef Platform_ADC_Stop_IT(Platform_ADC_Instance adc) {
  assert((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT);
  ADC_HandleTypeDef *hadc = platform_adc_to_hal(adc);
  assert(hadc != NULL);
  return platform_from_hal_status(HAL_ADC_Stop_IT(hadc));
}

Platform_StatusTypeDef Platform_ADC_Start_DMA(
  Platform_ADC_Instance adc,
  uint32_t *data,
  uint32_t length
) {
  assert(
    (uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT &&
    data != NULL &&
    length != 0U
  );
  if (!platform_adc_supports_dma(adc)) {
    return Platform_ERROR;
  }
  ADC_HandleTypeDef *hadc = platform_adc_to_hal(adc);
  assert(hadc != NULL);
  return platform_from_hal_status(HAL_ADC_Start_DMA(hadc, data, length));
}

Platform_StatusTypeDef Platform_ADC_Stop_DMA(Platform_ADC_Instance adc) {
  assert((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT);
  if (!platform_adc_supports_dma(adc)) {
    return Platform_ERROR;
  }
  ADC_HandleTypeDef *hadc = platform_adc_to_hal(adc);
  assert(hadc != NULL);
  return platform_from_hal_status(HAL_ADC_Stop_DMA(hadc));
}

uint32_t Platform_ADC_GetLastError(Platform_ADC_Instance adc) {
  assert((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT);
  ADC_HandleTypeDef *hadc = platform_adc_to_hal(adc);
  assert(hadc != NULL);
  return platform_adc_map_error(HAL_ADC_GetError(hadc));
}

void Platform_ADC_RegisterCallbacks(
  Platform_ADC_Instance adc,
  const platform_adc_callbacks_t *callbacks
) {
  assert(
    (uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT &&
    callbacks != NULL &&
    !adc_cb_registered[adc] &&
    platform_adc_to_hal(adc) != NULL
  );
  adc_cb[adc] = *callbacks;
  adc_cb_registered[adc] = true;
}

typedef enum {
  PLATFORM_ADC_EVENT_CONV_CPLT = 0,
  PLATFORM_ADC_EVENT_CONV_HALF,
  PLATFORM_ADC_EVENT_ERROR,
} platform_adc_event_t;

static void platform_adc_dispatch(
  ADC_HandleTypeDef *hadc,
  platform_adc_event_t event
) {
  Platform_ADC_Instance adc;
  if (!platform_hal_to_adc(hadc, &adc)) {
    return;
  }

  Platform_ADC_Callback callback = NULL;
  switch (event) {
    case PLATFORM_ADC_EVENT_CONV_CPLT:
      callback = adc_cb[adc].conv_cplt_cb;
      break;
    case PLATFORM_ADC_EVENT_CONV_HALF:
      callback = adc_cb[adc].conv_half_cplt_cb;
      break;
    case PLATFORM_ADC_EVENT_ERROR:
      callback = adc_cb[adc].err_cb;
      break;
    default:
      assert(0);
      break;
  }
  if (callback != NULL) {
    callback(adc, adc_cb[adc].context);
  }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
  platform_adc_dispatch(hadc, PLATFORM_ADC_EVENT_CONV_CPLT);
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
  platform_adc_dispatch(hadc, PLATFORM_ADC_EVENT_CONV_HALF);
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc) {
  platform_adc_dispatch(hadc, PLATFORM_ADC_EVENT_ERROR);
}
