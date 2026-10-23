/**
 * @file platform_crc_backend.h
 * @brief Private STM32-family backend for the common CRC implementation.
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_CRC_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_CRC_BACKEND_H

#include "platform_crc.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

bool platform_crc_backend_is_valid(Platform_CRC_Instance crc);

Platform_StatusTypeDef platform_crc_backend_reset(
  Platform_CRC_Instance crc
);

Platform_StatusTypeDef platform_crc_backend_accumulate(
  Platform_CRC_Instance crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
);

Platform_StatusTypeDef platform_crc_backend_calculate(
  Platform_CRC_Instance crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_CRC_BACKEND_H
