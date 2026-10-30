/**
 * @file platform_rng.c
 * @brief STM32F4 公共 RNG 实现的 HAL 后端。
 * @ingroup platform
 */

#include "platform_hal_internal.h"
#include "platform_rng_backend.h"
#include "rng.h"
#include "stm32f4xx_hal.h"

/**
 * @brief 把平台 RNG 实例身份映射到 CubeMX 生成的 HAL 句柄。
 * @param rng RNG 实例枚举值；当前仅 PLATFORM_RNG_1 映射到 hrng。
 * @return PLATFORM_RNG_1 对应 &hrng；其他值返回 NULL。
 * @note 纯映射，不访问 RNG 硬件；返回指针归 CubeMX/Core 所有。
 */
static RNG_HandleTypeDef *platform_rng_to_hal(platform_rng_instance_t rng) {
  return rng == PLATFORM_RNG_1 ? &hrng : NULL;
}

/**
 * @brief 把 HAL RNG 错误位映射为平台 RNG 错误位。
 * @param hal_error HAL_RNG_ERROR_* 位掩码。
 * @return 平台错误位组合；已知 TIMEOUT、BUSY、SEED、CLOCK 位逐一映射，
 *         未识别位合并为 UNKNOWN，无已知位时为 NONE。
 * @note 纯位映射，不访问硬件、不清除 HAL 快照，不分配内存。
 */
static uint32_t platform_rng_map_error(uint32_t hal_error) {
  uint32_t error = PLATFORM_RNG_ERROR_NONE;
  if ((hal_error & HAL_RNG_ERROR_TIMEOUT) != 0U) {
    error |= PLATFORM_RNG_ERROR_TIMEOUT;
  }
  if ((hal_error & HAL_RNG_ERROR_BUSY) != 0U) {
    error |= PLATFORM_RNG_ERROR_BUSY;
  }
  if ((hal_error & HAL_RNG_ERROR_SEED) != 0U) {
    error |= PLATFORM_RNG_ERROR_SEED;
  }
  if ((hal_error & HAL_RNG_ERROR_CLOCK) != 0U) {
    error |= PLATFORM_RNG_ERROR_CLOCK;
  }
  const uint32_t known_errors =
    HAL_RNG_ERROR_TIMEOUT |
    HAL_RNG_ERROR_BUSY |
    HAL_RNG_ERROR_SEED |
    HAL_RNG_ERROR_CLOCK;
  if ((hal_error & ~known_errors) != 0U) {
    error |= PLATFORM_RNG_ERROR_UNKNOWN;
  }
  return error;
}

/**
 * @brief 判断 RNG 实例是否映射到本系列的 HAL 句柄。
 * @param rng RNG 实例枚举值；当前仅支持 PLATFORM_RNG_1。
 * @retval true 实例为 PLATFORM_RNG_1，且本目标提供 hrng。
 * @retval false 实例不受支持。
 * @note 只查静态映射，不访问 RNG 硬件，也不读取错误状态。
 */
bool platform_rng_backend_is_valid(platform_rng_instance_t rng) {
  return platform_rng_to_hal(rng) != NULL;
}

/**
 * @brief 读取一个 32 位硬件随机数。
 * @param rng RNG 实例枚举值；当前仅支持 PLATFORM_RNG_1。
 * @param value 输出指针；成功时写入 32 位随机字，不得为 NULL。
 * @retval PLATFORM_STATUS_OK 已取得并写出随机数。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT rng 不受支持或 value 为 NULL。
 * @retval PLATFORM_STATUS_ERROR HAL 生成失败；具体超时、忙、种子或时钟原因由错误快照区分。
 * @pre MX_RNG_Init() 已完成；value 指向可写对象。
 * @post 成功时 value 被覆盖；失败时不保证修改输出，调用方不得使用。
 * @warning HAL 调用可能阻塞到内部超时，只允许在任务上下文调用。
 * @note HAL 超时与忙状态以 PLATFORM_STATUS_ERROR 返回，并通过 get_error 快照区分；
 *       不分配内存、不加锁，同一实例不得并发读取。
 */
platform_status_t platform_rng_backend_get_random(
  platform_rng_instance_t rng,
  uint32_t *value
) {
  RNG_HandleTypeDef *rng_h = platform_rng_to_hal(rng);
  return rng_h == NULL || value == NULL
           ? PLATFORM_STATUS_INVALID_ARGUMENT
           : platform_from_hal_status(
               HAL_RNG_GenerateRandomNumber(rng_h, value)
             );
}

/**
 * @brief 返回 HAL RNG 错误快照映射后的平台错误位。
 * @param rng RNG 实例枚举值；当前仅支持 PLATFORM_RNG_1。
 * @retval PLATFORM_RNG_ERROR_NONE HAL 快照没有已知错误位。
 * @retval PLATFORM_RNG_ERROR_UNKNOWN rng 不受支持，或 HAL 快照包含未识别错误位。
 * @post 读取不清除 HAL 错误快照。
 * @note 只读映射，不访问 RNG 数据寄存器，不分配内存。
 */
uint32_t platform_rng_backend_get_error(platform_rng_instance_t rng) {
  RNG_HandleTypeDef *rng_h = platform_rng_to_hal(rng);
  return rng_h == NULL ? PLATFORM_RNG_ERROR_UNKNOWN
                   : platform_rng_map_error(HAL_RNG_GetError(rng_h));
}
