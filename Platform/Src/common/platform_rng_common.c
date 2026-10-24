/**
 * @file platform_rng_common.c
 * @brief 与芯片系列无关的 RNG 参数校验与错误快照。
 * @ingroup platform
 */

#include "platform_error_internal.h"
#include "platform_rng_backend.h"
#include <stddef.h>

static volatile uint32_t rng_error;

platform_status_t Platform_RNG_GetRandom(
  platform_rng_instance_t rng,
  uint32_t *value
) {
  platform_error_reset(&rng_error);
  if (!platform_rng_backend_is_valid(rng) || value == NULL) {
    platform_error_record(&rng_error, PLATFORM_ERROR_INVALID_ARGUMENT);
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  platform_status_t status =
    platform_rng_backend_get_random(rng, value);
  if (status != PLATFORM_STATUS_OK) {
    platform_error_record(&rng_error, platform_error_from_status(status));
  }
  return status;
}

uint32_t Platform_RNG_GetLastError(platform_rng_instance_t rng) {
  if (!platform_rng_backend_is_valid(rng)) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return rng_error | platform_rng_backend_get_error(rng);
}
