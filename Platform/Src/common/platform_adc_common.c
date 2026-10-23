/**
 * @file platform_adc_common.c
 * @brief Family-independent ADC validation, errors and callback dispatch.
 */

#include "platform_adc_backend.h"
#include "platform_error_internal.h"
#include <stddef.h>

static volatile uint32_t adc_error[PLATFORM_ADC_INSTANCE_COUNT];
static platform_adc_callbacks_t adc_cb[PLATFORM_ADC_INSTANCE_COUNT];
static bool adc_cb_registered[PLATFORM_ADC_INSTANCE_COUNT];

static Platform_StatusTypeDef platform_adc_finish(
  Platform_ADC_Instance adc,
  Platform_StatusTypeDef status
) {
  if (status != Platform_OK) {
    platform_error_record(
      &adc_error[adc],
      platform_error_from_status(status)
    );
  }
  return status;
}

static Platform_StatusTypeDef platform_adc_invalid(Platform_ADC_Instance adc) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_record(
      &adc_error[adc],
      PLATFORM_COMMON_ERROR_PARAMETER
    );
  }
  return Platform_INVALID_ARGUMENT;
}

void platform_adc_dispatch_event(
  Platform_ADC_Instance adc,
  Platform_ADC_EventType type
) {
  if (
    (uint32_t)adc >= PLATFORM_ADC_INSTANCE_COUNT ||
    !adc_cb_registered[adc]
  ) {
    return;
  }

  Platform_ADC_Event event = {
    .type = type,
    .error = type == PLATFORM_ADC_EVENT_ERROR
               ? platform_adc_backend_get_error(adc)
               : PLATFORM_ADC_ERROR_NONE,
  };
  if (adc_cb[adc].callback != NULL) {
    adc_cb[adc].callback(adc, &event, adc_cb[adc].context);
  }
}

Platform_StatusTypeDef Platform_ADC_Start(Platform_ADC_Instance adc) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc)) {
    return platform_adc_invalid(adc);
  }
  return platform_adc_finish(adc, platform_adc_backend_start(adc));
}

Platform_StatusTypeDef Platform_ADC_Stop(Platform_ADC_Instance adc) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc)) {
    return platform_adc_invalid(adc);
  }
  return platform_adc_finish(adc, platform_adc_backend_stop(adc));
}

Platform_StatusTypeDef Platform_ADC_PollForConversion(
  Platform_ADC_Instance adc,
  uint32_t timeout
) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc)) {
    return platform_adc_invalid(adc);
  }
  return platform_adc_finish(adc, platform_adc_backend_poll(adc, timeout));
}

Platform_StatusTypeDef Platform_ADC_GetValue(
  Platform_ADC_Instance adc,
  uint32_t *value
) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc) || value == NULL) {
    return platform_adc_invalid(adc);
  }
  return platform_adc_finish(
    adc,
    platform_adc_backend_get_value(adc, value)
  );
}

Platform_StatusTypeDef Platform_ADC_Read(
  Platform_ADC_Instance adc,
  uint32_t *value,
  uint32_t timeout
) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc) || value == NULL) {
    return platform_adc_invalid(adc);
  }

  Platform_StatusTypeDef status = platform_adc_backend_start(adc);
  if (status != Platform_OK) {
    return platform_adc_finish(adc, status);
  }

  status = platform_adc_backend_poll(adc, timeout);
  if (status == Platform_OK) {
    status = platform_adc_backend_get_value(adc, value);
  }
  const Platform_StatusTypeDef stop_status =
    platform_adc_backend_stop(adc);
  if (status == Platform_OK) {
    status = stop_status;
  }
  return platform_adc_finish(adc, status);
}

Platform_StatusTypeDef Platform_ADC_Start_IT(Platform_ADC_Instance adc) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc)) {
    return platform_adc_invalid(adc);
  }
  return platform_adc_finish(adc, platform_adc_backend_start_it(adc));
}

Platform_StatusTypeDef Platform_ADC_Stop_IT(Platform_ADC_Instance adc) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc)) {
    return platform_adc_invalid(adc);
  }
  return platform_adc_finish(adc, platform_adc_backend_stop_it(adc));
}

Platform_StatusTypeDef Platform_ADC_Start_DMA(
  Platform_ADC_Instance adc,
  uint32_t *data,
  uint32_t length
) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (
    !platform_adc_backend_is_valid(adc) ||
    data == NULL ||
    length == 0U
  ) {
    return platform_adc_invalid(adc);
  }
  if (!platform_adc_backend_supports_dma(adc)) {
    platform_error_record(
      &adc_error[adc],
      PLATFORM_COMMON_ERROR_UNSUPPORTED
    );
    return Platform_UNSUPPORTED;
  }
  return platform_adc_finish(
    adc,
    platform_adc_backend_start_dma(adc, data, length)
  );
}

Platform_StatusTypeDef Platform_ADC_Stop_DMA(Platform_ADC_Instance adc) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc)) {
    return platform_adc_invalid(adc);
  }
  if (!platform_adc_backend_supports_dma(adc)) {
    platform_error_record(
      &adc_error[adc],
      PLATFORM_COMMON_ERROR_UNSUPPORTED
    );
    return Platform_UNSUPPORTED;
  }
  return platform_adc_finish(adc, platform_adc_backend_stop_dma(adc));
}

uint32_t Platform_ADC_GetLastError(Platform_ADC_Instance adc) {
  if ((uint32_t)adc >= PLATFORM_ADC_INSTANCE_COUNT) {
    return PLATFORM_COMMON_ERROR_PARAMETER;
  }
  return adc_error[adc] | platform_adc_backend_get_error(adc);
}

Platform_StatusTypeDef Platform_ADC_RegisterCallbacks(
  Platform_ADC_Instance adc,
  const platform_adc_callbacks_t *callbacks
) {
  if (
    (uint32_t)adc >= PLATFORM_ADC_INSTANCE_COUNT ||
    callbacks == NULL ||
    !platform_adc_backend_is_valid(adc)
  ) {
    return Platform_INVALID_ARGUMENT;
  }
  if (adc_cb_registered[adc]) {
    return Platform_BUSY;
  }

  adc_cb[adc] = *callbacks;
  adc_cb_registered[adc] = true;
  return Platform_OK;
}
