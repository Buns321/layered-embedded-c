/**
 * @file platform_rng_common.c
 * @brief Family-independent RNG validation and error snapshot.
 */

#include "platform_error_internal.h"
#include "platform_rng_backend.h"
#include <stddef.h>

static volatile uint32_t rng_error;

Platform_StatusTypeDef Platform_RNG_GetRandom(
  Platform_RNG_Instance rng,
  uint32_t *value
) {
  platform_error_reset(&rng_error);
  if (!platform_rng_backend_is_valid(rng) || value == NULL) {
    platform_error_record(&rng_error, PLATFORM_COMMON_ERROR_PARAMETER);
    return Platform_INVALID_ARGUMENT;
  }

  Platform_StatusTypeDef status =
    platform_rng_backend_get_random(rng, value);
  if (status != Platform_OK) {
    platform_error_record(&rng_error, platform_error_from_status(status));
  }
  return status;
}

uint32_t Platform_RNG_GetLastError(Platform_RNG_Instance rng) {
  if (!platform_rng_backend_is_valid(rng)) {
    return PLATFORM_COMMON_ERROR_PARAMETER;
  }
  return rng_error | platform_rng_backend_get_error(rng);
}
