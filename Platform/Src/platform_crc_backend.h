/**
 * @file platform_crc_backend.h
 * @brief STM32 系列公共 CRC 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_CRC_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_CRC_BACKEND_H

#include "platform_crc.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 校验CRC实例是否已由板级配置启用。
 */
bool platform_crc_backend_is_valid(platform_crc_instance_t crc);

/**
 * @brief 复位 CRC 计算状态。
 */
platform_status_t platform_crc_backend_reset(
  platform_crc_instance_t crc
);

/**
 * @brief 累计 CRC 输入数据块。
 */
platform_status_t platform_crc_backend_accumulate(
  platform_crc_instance_t crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
);

/**
 * @brief 按公共 CRC 配置计算校验值。
 */
platform_status_t platform_crc_backend_calculate(
  platform_crc_instance_t crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_CRC_BACKEND_H
