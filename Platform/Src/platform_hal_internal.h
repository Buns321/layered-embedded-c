//
// 由 Buns_ 创建于 2026/10/22。
//

/**
 * @file platform_hal_internal.h
 * @brief 仅供 Platform 实现使用的 HAL 状态转换辅助函数。
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_HAL_INTERNAL_H
#define LAYERED_EMBEDDED_C_PLATFORM_HAL_INTERNAL_H

#include "platform_def.h"
#include <assert.h>
#include <stdint.h>

static inline platform_status_t platform_from_hal_status(
  uint32_t hal_status
) {
  switch (hal_status) {
    case 0x00U: return PLATFORM_STATUS_OK;      /* HAL_OK */
    case 0x01U: return PLATFORM_STATUS_ERROR;   /* HAL_ERROR */
    case 0x02U: return PLATFORM_STATUS_BUSY;    /* HAL_BUSY */
    case 0x03U: return PLATFORM_STATUS_TIMEOUT; /* HAL_TIMEOUT */
    default:    assert(0); return PLATFORM_STATUS_ERROR;
  }
}

#endif //LAYERED_EMBEDDED_C_PLATFORM_HAL_INTERNAL_H
