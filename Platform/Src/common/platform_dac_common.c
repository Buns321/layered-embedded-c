/**
 * @file platform_dac_common.c
 * @brief Family-independent DAC validation, errors and callback dispatch.
 */

#include "platform_dac_backend.h"
#include "platform_error_internal.h"
#include <stddef.h>

static volatile uint32_t dac_error[PLATFORM_DAC_INSTANCE_COUNT];
static platform_dac_callbacks_t dac_cb[PLATFORM_DAC_INSTANCE_COUNT];
static bool dac_cb_registered[PLATFORM_DAC_INSTANCE_COUNT];

static Platform_StatusTypeDef platform_dac_finish(
  Platform_DAC_Instance dac,
  Platform_StatusTypeDef status
) {
  if (status != Platform_OK) {
    platform_error_record(
      &dac_error[dac],
      platform_error_from_status(status)
    );
  }
  return status;
}

static Platform_StatusTypeDef platform_dac_invalid(Platform_DAC_Instance dac) {
  if ((uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT) {
    platform_error_record(
      &dac_error[dac],
      PLATFORM_COMMON_ERROR_PARAMETER
    );
  }
  return Platform_INVALID_ARGUMENT;
}

static bool platform_dac_channel_valid(Platform_DAC_Channel channel) {
  return channel == PLATFORM_DAC_CHANNEL_1 ||
         channel == PLATFORM_DAC_CHANNEL_2;
}

static bool platform_dac_alignment_valid(Platform_DAC_Alignment alignment) {
  return alignment == PLATFORM_DAC_ALIGN_8_BIT_RIGHT ||
         alignment == PLATFORM_DAC_ALIGN_12_BIT_RIGHT ||
         alignment == PLATFORM_DAC_ALIGN_12_BIT_LEFT;
}

static bool platform_dac_value_valid(
  Platform_DAC_Alignment alignment,
  uint32_t value
) {
  return alignment == PLATFORM_DAC_ALIGN_8_BIT_RIGHT
           ? value <= 0xFFU
           : value <= 0xFFFU;
}

void platform_dac_dispatch_event(
  Platform_DAC_Instance dac,
  const Platform_DAC_Event *event
) {
  if (
    (uint32_t)dac >= PLATFORM_DAC_INSTANCE_COUNT ||
    event == NULL ||
    !dac_cb_registered[dac]
  ) {
    return;
  }
  if (dac_cb[dac].callback != NULL) {
    dac_cb[dac].callback(dac, event, dac_cb[dac].context);
  }
}

Platform_StatusTypeDef Platform_DAC_SetValue(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel,
  Platform_DAC_Alignment alignment,
  uint32_t value
) {
  if ((uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT) {
    platform_error_reset(&dac_error[dac]);
  }
  if (
    !platform_dac_backend_is_valid(dac) ||
    !platform_dac_channel_valid(channel) ||
    !platform_dac_alignment_valid(alignment) ||
    !platform_dac_value_valid(alignment, value)
  ) {
    return platform_dac_invalid(dac);
  }
  return platform_dac_finish(
    dac,
    platform_dac_backend_set_value(dac, channel, alignment, value)
  );
}

Platform_StatusTypeDef Platform_DAC_Start(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel
) {
  if ((uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT) {
    platform_error_reset(&dac_error[dac]);
  }
  if (
    !platform_dac_backend_is_valid(dac) ||
    !platform_dac_channel_valid(channel)
  ) {
    return platform_dac_invalid(dac);
  }
  return platform_dac_finish(
    dac,
    platform_dac_backend_start(dac, channel)
  );
}

Platform_StatusTypeDef Platform_DAC_Stop(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel
) {
  if ((uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT) {
    platform_error_reset(&dac_error[dac]);
  }
  if (
    !platform_dac_backend_is_valid(dac) ||
    !platform_dac_channel_valid(channel)
  ) {
    return platform_dac_invalid(dac);
  }
  return platform_dac_finish(
    dac,
    platform_dac_backend_stop(dac, channel)
  );
}

Platform_StatusTypeDef Platform_DAC_Start_DMA(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel,
  Platform_DAC_Alignment alignment,
  const uint32_t *data,
  uint32_t length
) {
  if ((uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT) {
    platform_error_reset(&dac_error[dac]);
  }
  if (
    !platform_dac_backend_is_valid(dac) ||
    !platform_dac_channel_valid(channel) ||
    !platform_dac_alignment_valid(alignment) ||
    data == NULL ||
    length == 0U
  ) {
    return platform_dac_invalid(dac);
  }
  return platform_dac_finish(
    dac,
    platform_dac_backend_start_dma(
      dac,
      channel,
      alignment,
      data,
      length
    )
  );
}

Platform_StatusTypeDef Platform_DAC_Stop_DMA(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel
) {
  if ((uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT) {
    platform_error_reset(&dac_error[dac]);
  }
  if (
    !platform_dac_backend_is_valid(dac) ||
    !platform_dac_channel_valid(channel)
  ) {
    return platform_dac_invalid(dac);
  }
  return platform_dac_finish(
    dac,
    platform_dac_backend_stop_dma(dac, channel)
  );
}

uint32_t Platform_DAC_GetLastError(Platform_DAC_Instance dac) {
  if ((uint32_t)dac >= PLATFORM_DAC_INSTANCE_COUNT) {
    return PLATFORM_COMMON_ERROR_PARAMETER;
  }
  return dac_error[dac] | platform_dac_backend_get_error(dac);
}

Platform_StatusTypeDef Platform_DAC_RegisterCallbacks(
  Platform_DAC_Instance dac,
  const platform_dac_callbacks_t *callbacks
) {
  if (
    (uint32_t)dac >= PLATFORM_DAC_INSTANCE_COUNT ||
    callbacks == NULL ||
    !platform_dac_backend_is_valid(dac)
  ) {
    return Platform_INVALID_ARGUMENT;
  }
  if (dac_cb_registered[dac]) {
    return Platform_BUSY;
  }

  dac_cb[dac] = *callbacks;
  dac_cb_registered[dac] = true;
  return Platform_OK;
}
