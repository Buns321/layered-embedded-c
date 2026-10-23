/**
 * @file platform_error_internal.h
 * @brief Common error snapshot helpers for Platform implementations.
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_ERROR_INTERNAL_H
#define LAYERED_EMBEDDED_C_PLATFORM_ERROR_INTERNAL_H

#include "platform_def.h"
#include <stddef.h>

static inline void platform_error_reset(volatile uint32_t *error) {
  if (error != NULL) {
    *error = PLATFORM_COMMON_ERROR_NONE;
  }
}

static inline void platform_error_record(
  volatile uint32_t *error,
  uint32_t flags
) {
  if (error != NULL) {
    *error |= flags;
  }
}

static inline uint32_t platform_error_from_status(
  Platform_StatusTypeDef status
) {
  switch (status) {
    case Platform_OK:
      return PLATFORM_COMMON_ERROR_NONE;
    case Platform_BUSY:
      return PLATFORM_COMMON_ERROR_BUSY;
    case Platform_TIMEOUT:
      return PLATFORM_COMMON_ERROR_TIMEOUT;
    case Platform_INVALID_ARGUMENT:
      return PLATFORM_COMMON_ERROR_PARAMETER;
    case Platform_UNSUPPORTED:
      return PLATFORM_COMMON_ERROR_UNSUPPORTED;
    case Platform_ERROR:
    default:
      return PLATFORM_COMMON_ERROR_HAL;
  }
}

#endif //LAYERED_EMBEDDED_C_PLATFORM_ERROR_INTERNAL_H
