//
// 由 Buns_ 创建于 2026/10/22。
//

#include "platform_rng.h"
#include "platform_hal_internal.h"
#include "rng.h"
#include "stm32f4xx_hal.h"
#include <assert.h>

static RNG_HandleTypeDef *platform_rng_to_hal(Platform_RNG_Instance rng) {
  assert(rng == PLATFORM_RNG_1);
  if (rng != PLATFORM_RNG_1) {
    return NULL;
  }

  return &hrng;
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

Platform_StatusTypeDef Platform_RNG_GetRandom(
  Platform_RNG_Instance rng,
  uint32_t *value
) {
  assert(value != NULL);

  RNG_HandleTypeDef *handle = platform_rng_to_hal(rng);
  assert(handle != NULL);

  return platform_from_hal_status(
    HAL_RNG_GenerateRandomNumber(handle, value)
  );
}

uint32_t Platform_RNG_GetLastError(Platform_RNG_Instance rng) {
  RNG_HandleTypeDef *handle = platform_rng_to_hal(rng);
  assert(handle != NULL);
  return platform_rng_map_error(HAL_RNG_GetError(handle));
}
