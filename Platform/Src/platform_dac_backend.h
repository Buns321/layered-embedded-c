/**
 * @file platform_dac_backend.h
 * @brief Private STM32-family backend for the common DAC implementation.
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_DAC_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_DAC_BACKEND_H

#include "platform_dac.h"
#include "platform_def.h"
#include <stdbool.h>

bool platform_dac_backend_is_valid(Platform_DAC_Instance dac);

Platform_StatusTypeDef platform_dac_backend_set_value(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel,
  Platform_DAC_Alignment alignment,
  uint32_t value
);

Platform_StatusTypeDef platform_dac_backend_start(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel
);

Platform_StatusTypeDef platform_dac_backend_stop(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel
);

Platform_StatusTypeDef platform_dac_backend_start_dma(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel,
  Platform_DAC_Alignment alignment,
  const uint32_t *data,
  uint32_t length
);

Platform_StatusTypeDef platform_dac_backend_stop_dma(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel
);

uint32_t platform_dac_backend_get_error(Platform_DAC_Instance dac);

void platform_dac_dispatch_event(
  Platform_DAC_Instance dac,
  const Platform_DAC_Event *event
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_DAC_BACKEND_H
