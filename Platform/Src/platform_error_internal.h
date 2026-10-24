/**
 * @file platform_error_internal.h
 * @brief Platform 实现共用的错误快照辅助函数。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_ERROR_INTERNAL_H
#define LAYERED_EMBEDDED_C_PLATFORM_ERROR_INTERNAL_H

#include "platform_def.h"
#include <stddef.h>

static inline void platform_error_reset(volatile uint32_t *error) {
  if (error != NULL) {
    *error = PLATFORM_ERROR_NONE;
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
  platform_status_t status
) {
  switch (status) {
    case PLATFORM_STATUS_OK:
      return PLATFORM_ERROR_NONE;
    case PLATFORM_STATUS_BUSY:
      return PLATFORM_ERROR_BUSY;
    case PLATFORM_STATUS_TIMEOUT:
      return PLATFORM_ERROR_TIMEOUT;
    case PLATFORM_STATUS_INVALID_ARGUMENT:
      return PLATFORM_ERROR_INVALID_ARGUMENT;
    case PLATFORM_STATUS_UNSUPPORTED:
      return PLATFORM_ERROR_UNSUPPORTED;
    case PLATFORM_STATUS_ERROR:
    default:
      return PLATFORM_ERROR_HAL;
  }
}

#endif //LAYERED_EMBEDDED_C_PLATFORM_ERROR_INTERNAL_H
