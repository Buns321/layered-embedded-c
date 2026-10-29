/**
 * @file platform_error_internal.h
 * @brief Platform 实现共用的错误快照辅助函数。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_ERROR_INTERNAL_H
#define LAYERED_EMBEDDED_C_PLATFORM_ERROR_INTERNAL_H

#include "platform_def.h"
#include <stddef.h>

/**
 * @brief 清除错误快照。
 * @param error 可空的错误位指针。
 */
static inline void platform_error_reset(volatile uint32_t *error) {
  if (error != NULL) {
    *error = PLATFORM_ERROR_NONE;
  }
}

/**
 * @brief 合并记录错误位。
 * @param error 可空的错误位指针。
 * @param flags 需要置位的错误位。
 */
static inline void platform_error_record(
  volatile uint32_t *error,
  uint32_t flags
) {
  if (error != NULL) {
    *error |= flags;
  }
}

/**
 * @brief 把平台状态码转换为通用错误位。
 * @param status 平台状态码。
 * @return 对应错误位；未知状态按 HAL 错误处理。
 */
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
