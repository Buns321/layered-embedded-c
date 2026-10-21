//
// Created by Buns_ on 2026/10/22.
//

/**
 * @file platform_hal_internal.h
 * @brief Private HAL status translation helpers for Platform implementations.
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_HAL_INTERNAL_H
#define LAYERED_EMBEDDED_C_PLATFORM_HAL_INTERNAL_H

#include "platform_def.h"
#include <assert.h>
#include <stdint.h>

static inline Platform_StatusTypeDef platform_from_hal_status(
  uint32_t hal_status
) {
  switch (hal_status) {
    case 0x00U: return Platform_OK;      /* HAL_OK */
    case 0x01U: return Platform_ERROR;   /* HAL_ERROR */
    case 0x02U: return Platform_BUSY;    /* HAL_BUSY */
    case 0x03U: return Platform_TIMEOUT; /* HAL_TIMEOUT */
    default:    assert(0); return Platform_ERROR;
  }
}

#endif //LAYERED_EMBEDDED_C_PLATFORM_HAL_INTERNAL_H
