//
// Created by Buns_ on 2026/10/23.
//

#include "platform_dac.h"
#include "platform_hal_internal.h"
#include "dac.h"
#include "stm32f4xx_hal.h"
#include <assert.h>
#include <stdbool.h>

static platform_dac_callbacks_t dac_cb[PLATFORM_DAC_INSTANCE_COUNT];
static bool dac_cb_registered[PLATFORM_DAC_INSTANCE_COUNT];

static DAC_HandleTypeDef *platform_dac_to_hal(Platform_DAC_Instance dac) {
  switch (dac) {
    case PLATFORM_DAC_1: return &hdac;
    default: assert(0); return NULL;
  }
}

static bool platform_hal_to_dac(
  DAC_HandleTypeDef *handle,
  Platform_DAC_Instance *dac
) {
  assert(handle != NULL && dac != NULL);
  if (handle == &hdac) {
    *dac = PLATFORM_DAC_1;
    return true;
  }
  return false;
}

static uint32_t platform_dac_hal_channel(Platform_DAC_Channel channel) {
  return channel == PLATFORM_DAC_CHANNEL_1
           ? DAC_CHANNEL_1
           : DAC_CHANNEL_2;
}

static uint32_t platform_dac_hal_alignment(
  Platform_DAC_Alignment alignment
) {
  switch (alignment) {
    case PLATFORM_DAC_ALIGN_8_BIT_RIGHT: return DAC_ALIGN_8B_R;
    case PLATFORM_DAC_ALIGN_12_BIT_RIGHT: return DAC_ALIGN_12B_R;
    case PLATFORM_DAC_ALIGN_12_BIT_LEFT: return DAC_ALIGN_12B_L;
    default: assert(0); return DAC_ALIGN_12B_R;
  }
}

static void platform_dac_validate_channel(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel
) {
  assert((uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT);
  assert(
    channel == PLATFORM_DAC_CHANNEL_1 ||
    channel == PLATFORM_DAC_CHANNEL_2
  );
  (void)dac;
  (void)channel;
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

Platform_StatusTypeDef Platform_DAC_SetValue(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel,
  Platform_DAC_Alignment alignment,
  uint32_t value
) {
  platform_dac_validate_channel(dac, channel);
  DAC_HandleTypeDef *hdac = platform_dac_to_hal(dac);
  assert(hdac != NULL);
  return platform_from_hal_status(HAL_DAC_SetValue(
    hdac,
    platform_dac_hal_channel(channel),
    platform_dac_hal_alignment(alignment),
    value
  ));
}

Platform_StatusTypeDef Platform_DAC_Start(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel
) {
  platform_dac_validate_channel(dac, channel);
  DAC_HandleTypeDef *hdac = platform_dac_to_hal(dac);
  assert(hdac != NULL);
  return platform_from_hal_status(HAL_DAC_Start(
    hdac,
    platform_dac_hal_channel(channel)
  ));
}

Platform_StatusTypeDef Platform_DAC_Stop(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel
) {
  platform_dac_validate_channel(dac, channel);
  DAC_HandleTypeDef *hdac = platform_dac_to_hal(dac);
  assert(hdac != NULL);
  return platform_from_hal_status(HAL_DAC_Stop(
    hdac,
    platform_dac_hal_channel(channel)
  ));
}

Platform_StatusTypeDef Platform_DAC_Start_DMA(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel,
  Platform_DAC_Alignment alignment,
  const uint32_t *data,
  uint32_t length
) {
  platform_dac_validate_channel(dac, channel);
  assert(data != NULL && length != 0U);
  DAC_HandleTypeDef *hdac = platform_dac_to_hal(dac);
  assert(hdac != NULL);
  return platform_from_hal_status(HAL_DAC_Start_DMA(
    hdac,
    platform_dac_hal_channel(channel),
    data,
    length,
    platform_dac_hal_alignment(alignment)
  ));
}

Platform_StatusTypeDef Platform_DAC_Stop_DMA(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel
) {
  platform_dac_validate_channel(dac, channel);
  DAC_HandleTypeDef *hdac = platform_dac_to_hal(dac);
  assert(hdac != NULL);
  return platform_from_hal_status(HAL_DAC_Stop_DMA(
    hdac,
    platform_dac_hal_channel(channel)
  ));
}

uint32_t Platform_DAC_GetLastError(Platform_DAC_Instance dac) {
  assert((uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT);
  DAC_HandleTypeDef *hdac = platform_dac_to_hal(dac);
  assert(hdac != NULL);
  return platform_dac_map_error(HAL_DAC_GetError(hdac));
}

void Platform_DAC_RegisterCallbacks(
  Platform_DAC_Instance dac,
  const platform_dac_callbacks_t *callbacks
) {
  assert(
    (uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT &&
    callbacks != NULL &&
    !dac_cb_registered[dac] &&
    platform_dac_to_hal(dac) != NULL
  );
  dac_cb[dac] = *callbacks;
  dac_cb_registered[dac] = true;
}

typedef enum {
  PLATFORM_DAC_EVENT_CONV_CPLT = 0,
  PLATFORM_DAC_EVENT_CONV_HALF,
  PLATFORM_DAC_EVENT_ERROR,
  PLATFORM_DAC_EVENT_UNDERRUN,
} platform_dac_event_t;

static void platform_dac_dispatch(
  DAC_HandleTypeDef *hdac,
  Platform_DAC_Channel channel,
  platform_dac_event_t event
) {
  Platform_DAC_Instance dac;
  if (!platform_hal_to_dac(hdac, &dac)) {
    return;
  }

  Platform_DAC_Callback callback = NULL;
  switch (event) {
    case PLATFORM_DAC_EVENT_CONV_CPLT:
      callback = dac_cb[dac].conv_cplt_cb;
      break;
    case PLATFORM_DAC_EVENT_CONV_HALF:
      callback = dac_cb[dac].conv_half_cplt_cb;
      break;
    case PLATFORM_DAC_EVENT_ERROR:
      callback = dac_cb[dac].err_cb;
      break;
    case PLATFORM_DAC_EVENT_UNDERRUN:
      callback = dac_cb[dac].underrun_cb;
      break;
    default:
      assert(0);
      break;
  }
  if (callback != NULL) {
    callback(dac, channel, dac_cb[dac].context);
  }
}

void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac) {
  platform_dac_dispatch(
    hdac,
    PLATFORM_DAC_CHANNEL_1,
    PLATFORM_DAC_EVENT_CONV_CPLT
  );
}

void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac) {
  platform_dac_dispatch(
    hdac,
    PLATFORM_DAC_CHANNEL_1,
    PLATFORM_DAC_EVENT_CONV_HALF
  );
}

void HAL_DAC_ErrorCallbackCh1(DAC_HandleTypeDef *hdac) {
  platform_dac_dispatch(
    hdac,
    PLATFORM_DAC_CHANNEL_1,
    PLATFORM_DAC_EVENT_ERROR
  );
}

void HAL_DAC_DMAUnderrunCallbackCh1(DAC_HandleTypeDef *hdac) {
  platform_dac_dispatch(
    hdac,
    PLATFORM_DAC_CHANNEL_1,
    PLATFORM_DAC_EVENT_UNDERRUN
  );
}

void HAL_DACEx_ConvCpltCallbackCh2(DAC_HandleTypeDef *hdac) {
  platform_dac_dispatch(
    hdac,
    PLATFORM_DAC_CHANNEL_2,
    PLATFORM_DAC_EVENT_CONV_CPLT
  );
}

void HAL_DACEx_ConvHalfCpltCallbackCh2(DAC_HandleTypeDef *hdac) {
  platform_dac_dispatch(
    hdac,
    PLATFORM_DAC_CHANNEL_2,
    PLATFORM_DAC_EVENT_CONV_HALF
  );
}

void HAL_DACEx_ErrorCallbackCh2(DAC_HandleTypeDef *hdac) {
  platform_dac_dispatch(
    hdac,
    PLATFORM_DAC_CHANNEL_2,
    PLATFORM_DAC_EVENT_ERROR
  );
}

void HAL_DACEx_DMAUnderrunCallbackCh2(DAC_HandleTypeDef *hdac) {
  platform_dac_dispatch(
    hdac,
    PLATFORM_DAC_CHANNEL_2,
    PLATFORM_DAC_EVENT_UNDERRUN
  );
}
