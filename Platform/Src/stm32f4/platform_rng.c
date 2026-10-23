/**
 * @file platform_rng.c
 * @brief STM32F4 HAL backend for the common RNG implementation.
 */

#include "platform_hal_internal.h"
#include "platform_rng_backend.h"
#include "rng.h"
#include "stm32f4xx_hal.h"

static RNG_HandleTypeDef *platform_rng_to_hal(platform_rng_instance_t rng) {
  return rng == PLATFORM_RNG_1 ? &hrng : NULL;
}

static uint32_t platform_rng_map_error(uint32_t hal_error) {
  uint32_t error = PLATFORM_RNG_ERROR_NONE;
  if ((hal_error & HAL_RNG_ERROR_TIMEOUT) != 0U) {
    error |= PLATFORM_RNG_ERROR_TIMEOUT;
  }
  if ((hal_error & HAL_RNG_ERROR_BUSY) != 0U) {
    error |= PLATFORM_RNG_ERROR_BUSY;
  }
  if ((hal_error & HAL_RNG_ERROR_SEED) != 0U) {
    error |= PLATFORM_RNG_ERROR_SEED;
  }
  if ((hal_error & HAL_RNG_ERROR_CLOCK) != 0U) {
    error |= PLATFORM_RNG_ERROR_CLOCK;
  }
  const uint32_t known_errors =
    HAL_RNG_ERROR_TIMEOUT |
    HAL_RNG_ERROR_BUSY |
    HAL_RNG_ERROR_SEED |
    HAL_RNG_ERROR_CLOCK;
  if ((hal_error & ~known_errors) != 0U) {
    error |= PLATFORM_RNG_ERROR_UNKNOWN;
  }
  return error;
}

bool platform_rng_backend_is_valid(platform_rng_instance_t rng) {
  return platform_rng_to_hal(rng) != NULL;
}

platform_status_t platform_rng_backend_get_random(
  platform_rng_instance_t rng,
  uint32_t *value
) {
  RNG_HandleTypeDef *rng_h = platform_rng_to_hal(rng);
  return rng_h == NULL || value == NULL
           ? PLATFORM_STATUS_INVALID_ARGUMENT
           : platform_from_hal_status(
               HAL_RNG_GenerateRandomNumber(rng_h, value)
             );
}

uint32_t platform_rng_backend_get_error(platform_rng_instance_t rng) {
  RNG_HandleTypeDef *rng_h = platform_rng_to_hal(rng);
  return rng_h == NULL ? PLATFORM_RNG_ERROR_UNKNOWN
                   : platform_rng_map_error(HAL_RNG_GetError(rng_h));
}
