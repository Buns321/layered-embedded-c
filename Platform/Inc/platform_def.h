//
// Created by Buns_ on 2026/8/15.
//

/**
 * @file platform_def.h
 * @brief Platform 层公共状态码。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_DEF_H
#define LAYERED_EMBEDDED_C_PLATFORM_DEF_H

#include <stdint.h>

/** @brief 永不超时的阻塞接口参数，单位为毫秒。 */
#define PLATFORM_TIMEOUT_INFINITE UINT32_MAX

/**
 * @brief Platform 层统一状态码。
 */
typedef enum {
  Platform_OK       = 0x00U,
  Platform_ERROR    = 0x01U,
  Platform_BUSY     = 0x02U,
  Platform_TIMEOUT  = 0x03U
} Platform_StatusTypeDef;

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_DEF_H
