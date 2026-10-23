/**
 * @file platform_dac.c
 * @brief STM32F4 HAL backend for the common DAC implementation.
 */

#include "dac.h"
#include "platform_dac_backend.h"
#include "platform_hal_internal.h"
#include "stm32f4xx_hal.h"

static DAC_HandleTypeDef *platform_dac_to_hal(Platform_DAC_Instance dac) {
  return dac == PLATFORM_DAC_1 ? &hdac : NULL;
}

static bool platform_hal_to_dac(
  DAC_HandleTypeDef *handle,
  Platform_DAC_Instance *dac
) {
  if (handle == &hdac && dac != NULL) {
    *dac = PLATFORM_DAC_1;
    return true;
  }
  return false;
}

static uint32_t platform_dac_hal_channel(Platform_DAC_Channel channel) {
  return channel == PLATFORM_DAC_CHANNEL_1 ? DAC_CHANNEL_1 : DAC_CHANNEL_2;
}

static uint32_t platform_dac_hal_alignment(
  Platform_DAC_Alignment alignment
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

bool platform_dac_backend_is_valid(Platform_DAC_Instance dac) {
  return platform_dac_to_hal(dac) != NULL;
}

Platform_StatusTypeDef platform_dac_backend_set_value(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel,
  Platform_DAC_Alignment alignment,
  uint32_t value
) {
  DAC_HandleTypeDef *h = platform_dac_to_hal(dac);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_DAC_SetValue(
                       h,
                       platform_dac_hal_channel(channel),
                       platform_dac_hal_alignment(alignment),
                       value
                     ));
}

Platform_StatusTypeDef platform_dac_backend_start(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel
) {
  DAC_HandleTypeDef *h = platform_dac_to_hal(dac);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_DAC_Start(
                       h,
                       platform_dac_hal_channel(channel)
                     ));
}

Platform_StatusTypeDef platform_dac_backend_stop(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel
) {
  DAC_HandleTypeDef *h = platform_dac_to_hal(dac);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_DAC_Stop(
                       h,
                       platform_dac_hal_channel(channel)
                     ));
}

Platform_StatusTypeDef platform_dac_backend_start_dma(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel,
  Platform_DAC_Alignment alignment,
  const uint32_t *data,
  uint32_t length
) {
  DAC_HandleTypeDef *h = platform_dac_to_hal(dac);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_DAC_Start_DMA(
                       h,
                       platform_dac_hal_channel(channel),
                       (uint32_t *)data,
                       length,
                       platform_dac_hal_alignment(alignment)
                     ));
}

Platform_StatusTypeDef platform_dac_backend_stop_dma(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel
) {
  DAC_HandleTypeDef *h = platform_dac_to_hal(dac);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_DAC_Stop_DMA(
                       h,
                       platform_dac_hal_channel(channel)
                     ));
}

uint32_t platform_dac_backend_get_error(Platform_DAC_Instance dac) {
  DAC_HandleTypeDef *h = platform_dac_to_hal(dac);
  return h == NULL ? PLATFORM_DAC_ERROR_UNKNOWN
                   : platform_dac_map_error(HAL_DAC_GetError(h));
}

static void platform_dac_dispatch_hal_event(
  DAC_HandleTypeDef *hdac,
  Platform_DAC_Channel channel,
  Platform_DAC_EventType type
) {
  Platform_DAC_Instance dac;
  if (!platform_hal_to_dac(hdac, &dac)) {
    return;
  }
  const Platform_DAC_Event event = {
    .type = type,
    .channel = channel,
    .error = type == PLATFORM_DAC_EVENT_ERROR ||
             type == PLATFORM_DAC_EVENT_UNDERRUN
               ? platform_dac_backend_get_error(dac)
               : PLATFORM_DAC_ERROR_NONE,
  };
  platform_dac_dispatch_event(dac, &event);
}

void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac) {
  platform_dac_dispatch_hal_event(
    hdac,
    PLATFORM_DAC_CHANNEL_1,
    PLATFORM_DAC_EVENT_CONV_CPLT
  );
}

void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac) {
  platform_dac_dispatch_hal_event(
    hdac,
    PLATFORM_DAC_CHANNEL_1,
    PLATFORM_DAC_EVENT_CONV_HALF
  );
}

void HAL_DAC_ErrorCallbackCh1(DAC_HandleTypeDef *hdac) {
  platform_dac_dispatch_hal_event(
    hdac,
    PLATFORM_DAC_CHANNEL_1,
    PLATFORM_DAC_EVENT_ERROR
  );
}

void HAL_DAC_DMAUnderrunCallbackCh1(DAC_HandleTypeDef *hdac) {
  platform_dac_dispatch_hal_event(
    hdac,
    PLATFORM_DAC_CHANNEL_1,
    PLATFORM_DAC_EVENT_UNDERRUN
  );
}

void HAL_DACEx_ConvCpltCallbackCh2(DAC_HandleTypeDef *hdac) {
  platform_dac_dispatch_hal_event(
    hdac,
    PLATFORM_DAC_CHANNEL_2,
    PLATFORM_DAC_EVENT_CONV_CPLT
  );
}

void HAL_DACEx_ConvHalfCpltCallbackCh2(DAC_HandleTypeDef *hdac) {
  platform_dac_dispatch_hal_event(
    hdac,
    PLATFORM_DAC_CHANNEL_2,
    PLATFORM_DAC_EVENT_CONV_HALF
  );
}

void HAL_DACEx_ErrorCallbackCh2(DAC_HandleTypeDef *hdac) {
  platform_dac_dispatch_hal_event(
    hdac,
    PLATFORM_DAC_CHANNEL_2,
    PLATFORM_DAC_EVENT_ERROR
  );
}

void HAL_DACEx_DMAUnderrunCallbackCh2(DAC_HandleTypeDef *hdac) {
  platform_dac_dispatch_hal_event(
    hdac,
    PLATFORM_DAC_CHANNEL_2,
    PLATFORM_DAC_EVENT_UNDERRUN
  );
}
