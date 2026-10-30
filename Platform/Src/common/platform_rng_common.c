/**
 * @file platform_rng_common.c
 * @brief 与芯片系列无关的 RNG 参数校验与错误快照。
 * @ingroup platform
 */

#include "platform_error_internal.h"
#include "platform_rng_backend.h"
#include <stddef.h>

static volatile uint32_t rng_error;

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
) {
  platform_error_reset(&rng_error);
  if (!platform_rng_backend_is_valid(rng) || value == NULL) {
    platform_error_record(&rng_error, PLATFORM_ERROR_INVALID_ARGUMENT);
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  platform_status_t status =
    platform_rng_backend_get_random(rng, value);
  if (status != PLATFORM_STATUS_OK) {
    platform_error_record(&rng_error, platform_error_from_status(status));
  }
  return status;
}

/**
 * @brief 读取最近一次 HAL RNG 错误快照。
 * @param rng RNG 实例；当前仅支持 PLATFORM_RNG_1，非法值返回 PLATFORM_ERROR_INVALID_ARGUMENT。
 * @return platform_rng_error_t 与 PLATFORM_ERROR_* 的位掩码组合；非法实例返回
 *         PLATFORM_ERROR_INVALID_ARGUMENT。
 * @note 读取不清除软件或硬件错误快照；后续 GetRandom 开始时清除软件快照。
 * @note 本函数可能读取 RNG 错误状态，但不会启动新的随机数生成。
 */
uint32_t Platform_RNG_GetLastError(platform_rng_instance_t rng) {
  if (!platform_rng_backend_is_valid(rng)) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return rng_error | platform_rng_backend_get_error(rng);
}
