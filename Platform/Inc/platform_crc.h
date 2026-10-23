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
#include <stdint.h>

#if !PLATFORM_ENABLE_CRC
#error "Platform CRC is not enabled"
#endif

/** @brief 可用的硬件 CRC 身份。 */
typedef enum {
  PLATFORM_CRC_1 = 0U,
} Platform_CRC_Instance;

/**
 * @brief 复位 CRC 数据寄存器。
 * @param crc CRC 实例。
 * @pre MX_CRC_Init() 已成功完成。
 */
void Platform_CRC_Reset(Platform_CRC_Instance crc);

/**
 * @brief 在不复位 CRC 单元的情况下累积计算 32 位字。
 * @param crc CRC 实例。
 * @param words 输入字数组。
 * @param word_count 32 位字数量，必须大于 0。
 * @return 累积计算后的 CRC 数据寄存器值。
 * @note STM32F4 的 CRC 单元按 32 位字处理数据。
 */
uint32_t Platform_CRC_AccumulateWords(
  Platform_CRC_Instance crc,
  const uint32_t *words,
  uint32_t word_count
);

/**
 * @brief 复位后计算 32 位字数组的 CRC。
 * @param crc CRC 实例。
 * @param words 输入字数组。
 * @param word_count 32 位字数量，必须大于 0。
 * @return 计算得到的 CRC 数据寄存器值。
 */
uint32_t Platform_CRC_CalculateWords(
  Platform_CRC_Instance crc,
  const uint32_t *words,
  uint32_t word_count
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_CRC_H
