/**
 * @file platform_dac_backend.h
 * @brief Private STM32-family backend for the common DAC implementation.
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_DAC_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_DAC_BACKEND_H

#include "platform_dac.h"
#include "platform_def.h"
#include <stdbool.h>

bool platform_dac_backend_is_valid(platform_dac_instance_t dac);

platform_status_t platform_dac_backend_set_value(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel,
  platform_dac_alignment_t alignment,
  uint32_t value
);

platform_status_t platform_dac_backend_start(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
);

platform_status_t platform_dac_backend_stop(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
);

platform_status_t platform_dac_backend_start_dma(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel,
  platform_dac_alignment_t alignment,
  const uint32_t *data,
  uint32_t length
);

platform_status_t platform_dac_backend_stop_dma(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
);

uint32_t platform_dac_backend_get_error(platform_dac_instance_t dac);

void platform_dac_dispatch_event(
  platform_dac_instance_t dac,
  const platform_dac_event_t *event
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_DAC_BACKEND_H
