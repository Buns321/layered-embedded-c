//
// Created by Buns_ on 2026/10/22.
//

/**
 * @file platform_rng.h
 * @brief HAL-free interface for the STM32 hardware random number generator.
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_RNG_H
#define LAYERED_EMBEDDED_C_PLATFORM_RNG_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

#if !PLATFORM_ENABLE_RNG
#error "Platform RNG is not enabled"
#endif

/** @brief Available hardware RNG identity. */
typedef enum {
  PLATFORM_RNG_1 = 0U,
} Platform_RNG_Instance;

/** @brief RNG error flags. */
typedef enum {
  PLATFORM_RNG_ERROR_NONE    = 0u,
  PLATFORM_RNG_ERROR_TIMEOUT = (1u << 0),
  PLATFORM_RNG_ERROR_BUSY    = (1u << 1),
  PLATFORM_RNG_ERROR_SEED    = (1u << 2),
  PLATFORM_RNG_ERROR_CLOCK   = (1u << 3),
  PLATFORM_RNG_ERROR_UNKNOWN = (1u << 7),
} Platform_RNG_Error;

/**
 * @brief Generate one 32-bit random number.
 * @param rng RNG instance.
 * @param value Output value.
 * @return Platform status.
 * @pre MX_RNG_Init() has completed successfully.
 * @note Uses the HAL RNG timeout and must be called from task context.
 */
Platform_StatusTypeDef Platform_RNG_GetRandom(
  Platform_RNG_Instance rng,
  uint32_t *value
);

/**
 * @brief Read the latest HAL RNG error snapshot.
 * @return Platform_RNG_Error bit mask.
 */
uint32_t Platform_RNG_GetLastError(Platform_RNG_Instance rng);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_RNG_H
