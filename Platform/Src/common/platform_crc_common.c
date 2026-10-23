/**
 * @file platform_crc_common.c
 * @brief Family-independent CRC validation and error snapshot.
 */

#include "platform_crc_backend.h"
#include "platform_error_internal.h"
#include <stddef.h>

static volatile uint32_t crc_error;

static Platform_StatusTypeDef platform_crc_invalid(void) {
  platform_error_record(&crc_error, PLATFORM_COMMON_ERROR_PARAMETER);
  return Platform_INVALID_ARGUMENT;
}

static Platform_StatusTypeDef platform_crc_finish(
  Platform_StatusTypeDef status
) {
  if (status != Platform_OK) {
    platform_error_record(&crc_error, platform_error_from_status(status));
  }
  return status;
}

Platform_StatusTypeDef Platform_CRC_Reset(Platform_CRC_Instance crc) {
  platform_error_reset(&crc_error);
  if (!platform_crc_backend_is_valid(crc)) {
    return platform_crc_invalid();
  }
  return platform_crc_finish(platform_crc_backend_reset(crc));
}

Platform_StatusTypeDef Platform_CRC_AccumulateWords(
  Platform_CRC_Instance crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
) {
  platform_error_reset(&crc_error);
  if (
    !platform_crc_backend_is_valid(crc) ||
    words == NULL ||
    word_count == 0U ||
    result == NULL
  ) {
    return platform_crc_invalid();
  }
  return platform_crc_finish(
    platform_crc_backend_accumulate(crc, words, word_count, result)
  );
}

Platform_StatusTypeDef Platform_CRC_CalculateWords(
  Platform_CRC_Instance crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
) {
  platform_error_reset(&crc_error);
  if (
    !platform_crc_backend_is_valid(crc) ||
    words == NULL ||
    word_count == 0U ||
    result == NULL
  ) {
    return platform_crc_invalid();
  }
  return platform_crc_finish(
    platform_crc_backend_calculate(crc, words, word_count, result)
  );
}

uint32_t Platform_CRC_GetLastError(Platform_CRC_Instance crc) {
  return platform_crc_backend_is_valid(crc)
           ? crc_error
           : PLATFORM_COMMON_ERROR_PARAMETER;
}
