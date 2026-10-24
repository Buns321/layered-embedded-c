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
typedef enum platform_status_e {
  PLATFORM_STATUS_OK       = 0x00U,
  PLATFORM_STATUS_ERROR    = 0x01U,
  PLATFORM_STATUS_BUSY     = 0x02U,
  PLATFORM_STATUS_TIMEOUT  = 0x03U,
  PLATFORM_STATUS_INVALID_ARGUMENT = 0x04U,
  PLATFORM_STATUS_UNSUPPORTED      = 0x05U,
} platform_status_t;

/**
 * @brief 所有 Platform 模块共用的错误标志。
 * @note 模块专用错误位不得与这些标志重叠。
 */
#define PLATFORM_ERROR_NONE        0u
#define PLATFORM_ERROR_INVALID_ARGUMENT   (1u << 20)
#define PLATFORM_ERROR_STATE       (1u << 21)
#define PLATFORM_ERROR_UNSUPPORTED (1u << 22)
#define PLATFORM_ERROR_TIMEOUT     (1u << 23)
#define PLATFORM_ERROR_BUSY        (1u << 24)
#define PLATFORM_ERROR_HAL         (1u << 25)
#define PLATFORM_ERROR_INTERNAL    (1u << 26)
#define PLATFORM_ERROR_UNKNOWN     (1u << 27)

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_DEF_H
