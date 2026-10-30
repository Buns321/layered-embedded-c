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
 * @param error 错误位存储地址；允许为 NULL。
 * @post error 非空时其值被写为 PLATFORM_ERROR_NONE，否则不产生副作用。
 * @note 不访问硬件、不分配内存；并发写入同一错误快照仍须由调用方串行化。
 */
static inline void platform_error_reset(volatile uint32_t *error) {
  if (error != NULL) {
    *error = PLATFORM_ERROR_NONE;
  }
}

/**
 * @brief 合并记录错误位。
 * @param error 错误位存储地址；允许为 NULL。
 * @param flags 要按位或写入的快照位，零表示不改变快照。
 * @post error 非空时保存原值或 flags；其他快照位保持不变。
 * @note 不访问硬件、不分配内存；对同一 error 的并发读改写须由调用方串行化。
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
 * @param status 待转换的平台操作状态。
 * @retval PLATFORM_ERROR_NONE status 为 PLATFORM_STATUS_OK，未记录错误。
 * @retval PLATFORM_ERROR_BUSY status 为 PLATFORM_STATUS_BUSY，操作暂不可执行。
 * @retval PLATFORM_ERROR_TIMEOUT status 为 PLATFORM_STATUS_TIMEOUT，操作未在限时内完成。
 * @retval PLATFORM_ERROR_INVALID_ARGUMENT status 为 PLATFORM_STATUS_INVALID_ARGUMENT，
 *         参数校验失败。
 * @retval PLATFORM_ERROR_UNSUPPORTED status 为 PLATFORM_STATUS_UNSUPPORTED，目标功能未实现。
 * @retval PLATFORM_ERROR_HAL status 为 PLATFORM_STATUS_ERROR 或其他未定义状态，表示底层失败。
 * @note 纯函数；失败语义由调用方结合原始 status 决定，本函数不访问硬件。
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
