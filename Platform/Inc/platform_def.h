//
// 由 Buns_ 创建于 2026/8/15。
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
  Platform_TIMEOUT  = 0x03U,
  Platform_INVALID_ARGUMENT = 0x04U,
  Platform_UNSUPPORTED      = 0x05U,
} Platform_StatusTypeDef;

/**
 * @brief Error flags shared by every Platform module.
 * @note Module-specific error bits must not overlap these flags.
 */
#define PLATFORM_COMMON_ERROR_NONE        0u
#define PLATFORM_COMMON_ERROR_PARAMETER   (1u << 20)
#define PLATFORM_COMMON_ERROR_STATE       (1u << 21)
#define PLATFORM_COMMON_ERROR_UNSUPPORTED (1u << 22)
#define PLATFORM_COMMON_ERROR_TIMEOUT     (1u << 23)
#define PLATFORM_COMMON_ERROR_BUSY        (1u << 24)
#define PLATFORM_COMMON_ERROR_HAL         (1u << 25)
#define PLATFORM_COMMON_ERROR_INTERNAL    (1u << 26)
#define PLATFORM_COMMON_ERROR_UNKNOWN     (1u << 27)

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_DEF_H
