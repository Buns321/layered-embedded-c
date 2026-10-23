//
// 由 Buns_ 创建于 2026/10/22。
//

/**
 * @file platform_crc.h
 * @brief STM32 硬件 CRC 单元的不依赖 HAL 类型接口。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_CRC_H
#define LAYERED_EMBEDDED_C_PLATFORM_CRC_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

#if !PLATFORM_ENABLE_CRC
#error "Platform CRC is not enabled"
#endif

/** @brief 可用的硬件 CRC 身份。 */
typedef enum platform_crc_instance_e {
  PLATFORM_CRC_1 = 0U,
} platform_crc_instance_t;

/**
 * @brief 复位 CRC 数据寄存器。
 * @param crc CRC 实例。
 * @pre MX_CRC_Init() 已成功完成。
 */
platform_status_t Platform_CRC_Reset(platform_crc_instance_t crc);

/**
 * @brief 在不复位 CRC 单元的情况下累积计算 32 位字。
 * @param crc CRC 实例。
 * @param words 输入字数组。
 * @param word_count 32 位字数量，必须大于 0。
 * @param result 累积计算后的 CRC 数据寄存器值输出。
 * @note STM32F4 的 CRC 单元按 32 位字处理数据。
 */
platform_status_t Platform_CRC_AccumulateWords(
  platform_crc_instance_t crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
);

/**
 * @brief 复位后计算 32 位字数组的 CRC。
 * @param crc CRC 实例。
 * @param words 输入字数组。
 * @param word_count 32 位字数量，必须大于 0。
 * @param result 计算得到的 CRC 数据寄存器值输出。
 */
platform_status_t Platform_CRC_CalculateWords(
  platform_crc_instance_t crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
);

/** @brief 读取最近一次 CRC 操作错误快照。 */
uint32_t Platform_CRC_GetLastError(platform_crc_instance_t crc);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_CRC_H
