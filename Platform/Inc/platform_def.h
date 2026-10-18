//
// Created by Buns_ on 2026/8/15.
//

#ifndef LAYERED_EMBEDDED_C_PLATFORM_DEF_H
#define LAYERED_EMBEDDED_C_PLATFORM_DEF_H

#include <assert.h>
#include <stdint.h>

typedef enum {
  Platform_OK       = 0x00U,
  Platform_ERROR    = 0x01U,
  Platform_BUSY     = 0x02U,
  Platform_TIMEOUT  = 0x03U
} Platform_StatusTypeDef;

/*
 * STM32 HAL uses the same status values across device families.
 * Keep HAL_StatusTypeDef out of the Platform interface.
 */
static inline Platform_StatusTypeDef hal_to_platform_statustypedef(uint32_t statustypedef) {
  switch (statustypedef) {
    case 0x00U: return Platform_OK;      /* HAL_OK */
    case 0x01U: return Platform_ERROR;   /* HAL_ERROR */
    case 0x02U: return Platform_BUSY;    /* HAL_BUSY */
    case 0x03U: return Platform_TIMEOUT; /* HAL_TIMEOUT */
    default:    assert(0); return Platform_ERROR;
  }
}

#endif //LAYERED_EMBEDDED_C_PLATFORM_DEF_H
