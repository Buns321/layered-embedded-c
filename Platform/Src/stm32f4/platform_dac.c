/**
 * @file platform_dac.c
 * @brief STM32F4 HAL backend for the common DAC implementation.
 */

#include "dac.h"
#include "platform_dac_backend.h"
#include "platform_hal_internal.h"
#include "stm32f4xx_hal.h"

static DAC_HandleTypeDef *platform_dac_to_hal(platform_dac_instance_t dac) {
  return dac == PLATFORM_DAC_1 ? &hdac : NULL;
}

static bool platform_hal_to_dac(
  DAC_HandleTypeDef *dac_h,
  platform_dac_instance_t *dac
) {
  if (dac_h == &hdac && dac != NULL) {
    *dac = PLATFORM_DAC_1;
    return true;
  }
  return false;
}

static uint32_t platform_dac_hal_channel(platform_dac_channel_t channel) {
  return channel == PLATFORM_DAC_CHANNEL_1 ? DAC_CHANNEL_1 : DAC_CHANNEL_2;
}

static uint32_t platform_dac_hal_alignment(
  platform_dac_alignment_t alignment
) {
  switch (alignment) {
    case PLATFORM_DAC_ALIGN_8_BIT_RIGHT: return DAC_ALIGN_8B_R;
    case PLATFORM_DAC_ALIGN_12_BIT_RIGHT: return DAC_ALIGN_12B_R;
    case PLATFORM_DAC_ALIGN_12_BIT_LEFT: return DAC_ALIGN_12B_L;
    default: return DAC_ALIGN_12B_R;
  }
}

static uint32_t platform_dac_map_error(uint32_t hal_error) {
  uint32_t error = PLATFORM_DAC_ERROR_NONE;
  if (((hal_error & HAL_DAC_ERROR_DMAUNDERRUNCH1) != 0U) ||
      ((hal_error & HAL_DAC_ERROR_DMAUNDERRUNCH2) != 0U)) {
    error |= PLATFORM_DAC_ERROR_DMA_UNDERRUN;
  }
  if ((hal_error & HAL_DAC_ERROR_DMA) != 0U) {
    error |= PLATFORM_DAC_ERROR_DMA;
  }
  if ((hal_error & HAL_DAC_ERROR_TIMEOUT) != 0U) {
    error |= PLATFORM_DAC_ERROR_TIMEOUT;
  }
  const uint32_t known_errors =
    HAL_DAC_ERROR_DMAUNDERRUNCH1 |
    HAL_DAC_ERROR_DMAUNDERRUNCH2 |
    HAL_DAC_ERROR_DMA |
    HAL_DAC_ERROR_TIMEOUT;
  if ((hal_error & ~known_errors) != 0U) {
    error |= PLATFORM_DAC_ERROR_UNKNOWN;
  }
  return error;
}

bool platform_dac_backend_is_valid(platform_dac_instance_t dac) {
  return platform_dac_to_hal(dac) != NULL;
}

platform_status_t platform_dac_backend_set_value(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel,
  platform_dac_alignment_t alignment,
  uint32_t value
) {
  DAC_HandleTypeDef *dac_h = platform_dac_to_hal(dac);
  return dac_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_DAC_SetValue(
                       dac_h,
                       platform_dac_hal_channel(channel),
                       platform_dac_hal_alignment(alignment),
                       value
                     ));
}

platform_status_t platform_dac_backend_start(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
) {
  DAC_HandleTypeDef *dac_h = platform_dac_to_hal(dac);
  return dac_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_DAC_Start(
                       dac_h,
                       platform_dac_hal_channel(channel)
                     ));
}

platform_status_t platform_dac_backend_stop(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
) {
  DAC_HandleTypeDef *dac_h = platform_dac_to_hal(dac);
  return dac_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_DAC_Stop(
                       dac_h,
                       platform_dac_hal_channel(channel)
                     ));
}

platform_status_t platform_dac_backend_start_dma(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel,
  platform_dac_alignment_t alignment,
  const uint32_t *data,
  uint32_t length
) {
  DAC_HandleTypeDef *dac_h = platform_dac_to_hal(dac);
  return dac_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_DAC_Start_DMA(
                       dac_h,
                       platform_dac_hal_channel(channel),
                       (uint32_t *)data,
                       length,
                       platform_dac_hal_alignment(alignment)
                     ));
}

platform_status_t platform_dac_backend_stop_dma(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
) {
  DAC_HandleTypeDef *dac_h = platform_dac_to_hal(dac);
  return dac_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_DAC_Stop_DMA(
                       dac_h,
                       platform_dac_hal_channel(channel)
                     ));
}

uint32_t platform_dac_backend_get_error(platform_dac_instance_t dac) {
  DAC_HandleTypeDef *dac_h = platform_dac_to_hal(dac);
  return dac_h == NULL ? PLATFORM_DAC_ERROR_UNKNOWN
                   : platform_dac_map_error(HAL_DAC_GetError(dac_h));
}

static void platform_dac_dispatch_hal_event(
  DAC_HandleTypeDef *dac_h,
  platform_dac_channel_t channel,
  platform_dac_event_type_t type
) {
  platform_dac_instance_t dac;
  if (!platform_hal_to_dac(dac_h, &dac)) {
    return;
  }
  const platform_dac_event_t event = {
    .type = type,
    .channel = channel,
    .error = type == PLATFORM_DAC_EVENT_ERROR ||
             type == PLATFORM_DAC_EVENT_UNDERRUN
               ? platform_dac_backend_get_error(dac)
               : PLATFORM_DAC_ERROR_NONE,
  };
  platform_dac_dispatch_event(dac, &event);
}

void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *dac_h) {
  platform_dac_dispatch_hal_event(
    dac_h,
    PLATFORM_DAC_CHANNEL_1,
    PLATFORM_DAC_EVENT_CONV_COMPLETE
  );
}

void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *dac_h) {
  platform_dac_dispatch_hal_event(
    dac_h,
    PLATFORM_DAC_CHANNEL_1,
    PLATFORM_DAC_EVENT_CONV_HALF_COMPLETE
  );
}

void HAL_DAC_ErrorCallbackCh1(DAC_HandleTypeDef *dac_h) {
  platform_dac_dispatch_hal_event(
    dac_h,
    PLATFORM_DAC_CHANNEL_1,
    PLATFORM_DAC_EVENT_ERROR
  );
}

void HAL_DAC_DMAUnderrunCallbackCh1(DAC_HandleTypeDef *dac_h) {
  platform_dac_dispatch_hal_event(
    dac_h,
    PLATFORM_DAC_CHANNEL_1,
    PLATFORM_DAC_EVENT_UNDERRUN
  );
}

void HAL_DACEx_ConvCpltCallbackCh2(DAC_HandleTypeDef *dac_h) {
  platform_dac_dispatch_hal_event(
    dac_h,
    PLATFORM_DAC_CHANNEL_2,
    PLATFORM_DAC_EVENT_CONV_COMPLETE
  );
}

void HAL_DACEx_ConvHalfCpltCallbackCh2(DAC_HandleTypeDef *dac_h) {
  platform_dac_dispatch_hal_event(
    dac_h,
    PLATFORM_DAC_CHANNEL_2,
    PLATFORM_DAC_EVENT_CONV_HALF_COMPLETE
  );
}

void HAL_DACEx_ErrorCallbackCh2(DAC_HandleTypeDef *dac_h) {
  platform_dac_dispatch_hal_event(
    dac_h,
    PLATFORM_DAC_CHANNEL_2,
    PLATFORM_DAC_EVENT_ERROR
  );
}

void HAL_DACEx_DMAUnderrunCallbackCh2(DAC_HandleTypeDef *dac_h) {
  platform_dac_dispatch_hal_event(
    dac_h,
    PLATFORM_DAC_CHANNEL_2,
    PLATFORM_DAC_EVENT_UNDERRUN
  );
}
