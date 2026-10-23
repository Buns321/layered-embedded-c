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

bool platform_crc_backend_is_valid(platform_crc_instance_t crc);

platform_status_t platform_crc_backend_reset(
  platform_crc_instance_t crc
);

platform_status_t platform_crc_backend_accumulate(
  platform_crc_instance_t crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
);

platform_status_t platform_crc_backend_calculate(
  platform_crc_instance_t crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_CRC_BACKEND_H
