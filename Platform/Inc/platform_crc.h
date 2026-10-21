//
// Created by Buns_ on 2026/10/22.
//

/**
 * @file platform_crc.h
 * @brief HAL-free interface for the STM32 hardware CRC unit.
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

/** @brief Available hardware CRC identity. */
typedef enum {
  PLATFORM_CRC_1 = 0U,
} Platform_CRC_Instance;

/**
 * @brief Reset the CRC data register.
 * @param crc CRC instance.
 * @pre MX_CRC_Init() has completed successfully.
 */
void Platform_CRC_Reset(Platform_CRC_Instance crc);

/**
 * @brief Accumulate 32-bit words without resetting the CRC unit.
 * @param crc CRC instance.
 * @param words Input words.
 * @param word_count Number of 32-bit words, must be greater than 0.
 * @return CRC data register after accumulation.
 * @note The STM32F4 CRC unit consumes 32-bit words.
 */
uint32_t Platform_CRC_AccumulateWords(
  Platform_CRC_Instance crc,
  const uint32_t *words,
  uint32_t word_count
);

/**
 * @brief Reset and calculate the CRC over 32-bit words.
 * @param crc CRC instance.
 * @param words Input words.
 * @param word_count Number of 32-bit words, must be greater than 0.
 * @return Calculated CRC data register.
 */
uint32_t Platform_CRC_CalculateWords(
  Platform_CRC_Instance crc,
  const uint32_t *words,
  uint32_t word_count
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_CRC_H
