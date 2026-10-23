/**
 * @file platform_encoder_backend.h
 * @brief Private STM32-family backend for the common encoder implementation.
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_ENCODER_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_ENCODER_BACKEND_H

#include "platform_def.h"
#include "platform_encoder.h"
#include <stdbool.h>
#include <stdint.h>

bool platform_encoder_backend_is_valid(Platform_Encoder_Instance encoder);

Platform_StatusTypeDef platform_encoder_backend_start(
  Platform_Encoder_Instance encoder
);

Platform_StatusTypeDef platform_encoder_backend_stop(
  Platform_Encoder_Instance encoder
);

Platform_StatusTypeDef platform_encoder_backend_get_counter(
  Platform_Encoder_Instance encoder,
  uint32_t *counter
);

Platform_StatusTypeDef platform_encoder_backend_set_counter(
  Platform_Encoder_Instance encoder,
  uint32_t counter
);

Platform_StatusTypeDef platform_encoder_backend_get_counter_bits(
  Platform_Encoder_Instance encoder,
  uint8_t *counter_bits
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_ENCODER_BACKEND_H
