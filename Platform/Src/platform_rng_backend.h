/**
 * @file platform_rng_backend.h
 * @brief Private STM32-family backend for the common RNG implementation.
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_RNG_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_RNG_BACKEND_H

#include "platform_def.h"
#include "platform_rng.h"
#include <stdbool.h>
#include <stdint.h>

bool platform_rng_backend_is_valid(Platform_RNG_Instance rng);

Platform_StatusTypeDef platform_rng_backend_get_random(
  Platform_RNG_Instance rng,
  uint32_t *value
);

uint32_t platform_rng_backend_get_error(Platform_RNG_Instance rng);

#endif //LAYERED_EMBEDDED_C_PLATFORM_RNG_BACKEND_H
