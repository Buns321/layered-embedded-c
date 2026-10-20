//
// Created by Buns_ on 2026/8/15.
//

/**
 * @file platform_def.h
 * @brief Platform 层公共状态码与 HAL 状态映射。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_DEF_H
#define LAYERED_EMBEDDED_C_PLATFORM_DEF_H

#include <assert.h>
#include <stdint.h>

/**
 * @brief Platform 层统一状态码。
 */
typedef enum {
  Platform_OK       = 0x00U,
  Platform_ERROR    = 0x01U,
  Platform_BUSY     = 0x02U,
  Platform_TIMEOUT  = 0x03U
} Platform_StatusTypeDef;

/**
 * @brief 将 HAL 状态码映射为 Platform 状态码。
 * @param statustypedef HAL 状态码的数值。
 * @retval Platform_OK 成功。
 * @retval Platform_ERROR 错误。
 * @retval Platform_BUSY 忙。
 * @retval Platform_TIMEOUT 超时。
 * @note 不把 HAL_StatusTypeDef 引入 Platform 接口。
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

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_DEF_H
