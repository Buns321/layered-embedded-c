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
 * @param rng 已启用的硬件 RNG 实例。
 * @param value 随机数输出地址；只在成功时保证更新。
 * @retval PLATFORM_STATUS_OK value 已写入一个 32 位随机数。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或 value 为空。
 * @retval PLATFORM_STATUS_BUSY RNG 正忙。
 * @retval PLATFORM_STATUS_TIMEOUT HAL 等待随机数超时。
 * @retval PLATFORM_STATUS_ERROR 种子、时钟或其他硬件错误。
 * @pre MX_RNG_Init() 已成功完成。
 * @note 使用 HAL RNG 的超时机制，必须从任务上下文调用。
 * @note 同一 RNG 实例不可并发调用；value 所有权仍归调用方。
 */
platform_status_t Platform_RNG_GetRandom(
  platform_rng_instance_t rng,
  uint32_t *value
);

/**
 * @brief 读取最近一次 HAL RNG 错误快照。
 * @param rng RNG 实例；当前仅支持 PLATFORM_RNG_1，非法值返回 PLATFORM_ERROR_INVALID_ARGUMENT。
 * @return platform_rng_error_t 与 PLATFORM_ERROR_* 的位掩码组合；非法实例返回
 *         PLATFORM_ERROR_INVALID_ARGUMENT。
 * @note 读取不清除软件或硬件错误快照；后续 GetRandom 开始时清除软件快照。
 * @note 本函数可能读取 RNG 错误状态，但不会启动新的随机数生成。
 */
uint32_t Platform_RNG_GetLastError(platform_rng_instance_t rng);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_RNG_H
