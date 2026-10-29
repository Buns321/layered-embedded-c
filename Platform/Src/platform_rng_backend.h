/**
 * @file platform_rng_backend.h
 * @brief STM32 系列公共 RNG 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_RNG_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_RNG_BACKEND_H

#include "platform_def.h"
#include "platform_rng.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 校验RNG实例是否已由板级配置启用。
 */
bool platform_rng_backend_is_valid(platform_rng_instance_t rng);

/**
 * @brief 读取随机数生成器输出。
 */
platform_status_t platform_rng_backend_get_random(
  platform_rng_instance_t rng,
  uint32_t *value
);

/**
 * @brief 返回RNG后端记录的错误位。
 */
uint32_t platform_rng_backend_get_error(platform_rng_instance_t rng);

#endif //LAYERED_EMBEDDED_C_PLATFORM_RNG_BACKEND_H
