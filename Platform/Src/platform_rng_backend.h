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
 * @brief 判断 RNG 实例是否映射到本系列的 HAL 句柄。
 * @param rng RNG 实例枚举值；当前仅支持 PLATFORM_RNG_1。
 * @retval true 实例为 PLATFORM_RNG_1，且本目标提供 hrng。
 * @retval false 实例不受支持。
 * @note 只查静态映射，不访问 RNG 硬件，也不读取错误状态。
 */
bool platform_rng_backend_is_valid(platform_rng_instance_t rng);

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
);

/**
 * @brief 返回 HAL RNG 错误快照映射后的平台错误位。
 * @param rng RNG 实例枚举值；当前仅支持 PLATFORM_RNG_1。
 * @retval PLATFORM_RNG_ERROR_NONE HAL 快照没有已知错误位。
 * @retval PLATFORM_RNG_ERROR_UNKNOWN rng 不受支持，或 HAL 快照包含未识别错误位。
 * @post 读取不清除 HAL 错误快照。
 * @note 只读映射，不访问 RNG 数据寄存器，不分配内存。
 */
uint32_t platform_rng_backend_get_error(platform_rng_instance_t rng);

#endif //LAYERED_EMBEDDED_C_PLATFORM_RNG_BACKEND_H
