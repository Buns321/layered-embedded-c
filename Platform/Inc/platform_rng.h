//
// 由 Buns_ 创建于 2026/10/22。
//

/**
 * @file platform_rng.h
 * @brief STM32 硬件随机数生成器的不依赖 HAL 类型接口。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_RNG_H
#define LAYERED_EMBEDDED_C_PLATFORM_RNG_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

#if !PLATFORM_ENABLE_RNG
#error "Platform RNG is not enabled"
#endif

/** @brief 可用的硬件 RNG 身份。 */
typedef enum platform_rng_instance_e {
  PLATFORM_RNG_1 = 0U,
} platform_rng_instance_t;

/** @brief RNG 错误标志。 */
typedef enum platform_rng_error_e {
  PLATFORM_RNG_ERROR_NONE    = 0u,
  PLATFORM_RNG_ERROR_TIMEOUT = (1u << 0),
  PLATFORM_RNG_ERROR_BUSY    = (1u << 1),
  PLATFORM_RNG_ERROR_SEED    = (1u << 2),
  PLATFORM_RNG_ERROR_CLOCK   = (1u << 3),
  PLATFORM_RNG_ERROR_UNKNOWN = (1u << 7),
} platform_rng_error_t;

/**
 * @brief 生成一个 32 位随机数。
 * @param rng RNG 实例。
 * @param value 输出值。
 * @return Platform 状态。
 * @pre MX_RNG_Init() 已成功完成。
 * @note 使用 HAL RNG 的超时机制，必须从任务上下文调用。
 */
platform_status_t Platform_RNG_GetRandom(
  platform_rng_instance_t rng,
  uint32_t *value
);

/**
 * @brief 读取最近一次 HAL RNG 错误快照。
 * @return platform_rng_error_t 位掩码。
 */
uint32_t Platform_RNG_GetLastError(platform_rng_instance_t rng);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_RNG_H
