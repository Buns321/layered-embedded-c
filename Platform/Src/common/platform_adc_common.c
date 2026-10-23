/**
 * @file platform_adc_common.c
 * @brief Family-independent ADC validation, errors and callback dispatch.
 */

#include "platform_adc_backend.h"
#include "platform_error_internal.h"
#include <stddef.h>

static volatile uint32_t adc_error[PLATFORM_ADC_INSTANCE_COUNT];
static platform_adc_callback_binding_t adc_cb[PLATFORM_ADC_INSTANCE_COUNT];
static bool adc_cb_registered[PLATFORM_ADC_INSTANCE_COUNT];

static platform_status_t platform_adc_finish(
  platform_adc_instance_t adc,
  platform_status_t status
) {
  if (status != PLATFORM_STATUS_OK) {
    platform_error_record(
      &adc_error[adc],
      platform_error_from_status(status)
    );
  }
  return status;
}

static platform_status_t platform_adc_invalid(platform_adc_instance_t adc) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_record(
      &adc_error[adc],
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
  }
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

void platform_adc_dispatch_event(
  platform_adc_instance_t adc,
  platform_adc_event_type_t type
) {
  if (
    (uint32_t)adc >= PLATFORM_ADC_INSTANCE_COUNT ||
    !adc_cb_registered[adc]
  ) {
    return;
  }

  platform_adc_event_t event = {
    .type = type,
    .error = type == PLATFORM_ADC_EVENT_ERROR
               ? platform_adc_backend_get_error(adc)
               : PLATFORM_ADC_ERROR_NONE,
  };
  if (adc_cb[adc].callback != NULL) {
    adc_cb[adc].callback(adc, &event, adc_cb[adc].context);
  }
}

platform_status_t Platform_ADC_Start(platform_adc_instance_t adc) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc)) {
    return platform_adc_invalid(adc);
  }
  return platform_adc_finish(adc, platform_adc_backend_start(adc));
}

platform_status_t Platform_ADC_Stop(platform_adc_instance_t adc) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc)) {
    return platform_adc_invalid(adc);
  }
  return platform_adc_finish(adc, platform_adc_backend_stop(adc));
}

platform_status_t Platform_ADC_PollForConversion(
  platform_adc_instance_t adc,
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

platform_status_t Platform_ADC_GetValue(
  platform_adc_instance_t adc,
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

platform_status_t Platform_ADC_Read(
  platform_adc_instance_t adc,
  uint32_t *value,
  uint32_t timeout
) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc) || value == NULL) {
    return platform_adc_invalid(adc);
  }

  platform_status_t status = platform_adc_backend_start(adc);
  if (status != PLATFORM_STATUS_OK) {
    return platform_adc_finish(adc, status);
  }

  status = platform_adc_backend_poll(adc, timeout);
  if (status == PLATFORM_STATUS_OK) {
    status = platform_adc_backend_get_value(adc, value);
  }
  const platform_status_t stop_status =
    platform_adc_backend_stop(adc);
  if (status == PLATFORM_STATUS_OK) {
    status = stop_status;
  }
  return platform_adc_finish(adc, status);
}

platform_status_t Platform_ADC_StartIT(platform_adc_instance_t adc) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc)) {
    return platform_adc_invalid(adc);
  }
  return platform_adc_finish(adc, platform_adc_backend_start_it(adc));
}

platform_status_t Platform_ADC_StopIT(platform_adc_instance_t adc) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc)) {
    return platform_adc_invalid(adc);
  }
  return platform_adc_finish(adc, platform_adc_backend_stop_it(adc));
}

platform_status_t Platform_ADC_StartDMA(
  platform_adc_instance_t adc,
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
      PLATFORM_ERROR_UNSUPPORTED
    );
    return PLATFORM_STATUS_UNSUPPORTED;
  }
  return platform_adc_finish(
    adc,
    platform_adc_backend_start_dma(adc, data, length)
  );
}

platform_status_t Platform_ADC_StopDMA(platform_adc_instance_t adc) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc)) {
    return platform_adc_invalid(adc);
  }
  if (!platform_adc_backend_supports_dma(adc)) {
    platform_error_record(
      &adc_error[adc],
      PLATFORM_ERROR_UNSUPPORTED
    );
    return PLATFORM_STATUS_UNSUPPORTED;
  }
  return platform_adc_finish(adc, platform_adc_backend_stop_dma(adc));
}

uint32_t Platform_ADC_GetLastError(platform_adc_instance_t adc) {
  if ((uint32_t)adc >= PLATFORM_ADC_INSTANCE_COUNT) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return adc_error[adc] | platform_adc_backend_get_error(adc);
}

platform_status_t Platform_ADC_RegisterCallback(
  platform_adc_instance_t adc,
  const platform_adc_callback_binding_t *callbacks
) {
  if (
    (uint32_t)adc >= PLATFORM_ADC_INSTANCE_COUNT ||
    callbacks == NULL ||
    !platform_adc_backend_is_valid(adc)
  ) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (adc_cb_registered[adc]) {
    return PLATFORM_STATUS_BUSY;
  }

  adc_cb[adc] = *callbacks;
  adc_cb_registered[adc] = true;
  return PLATFORM_STATUS_OK;
}
