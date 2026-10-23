/**
 * @file platform_crc_common.c
 * @brief Family-independent CRC validation and error snapshot.
 */

#include "platform_crc_backend.h"
#include "platform_error_internal.h"
#include <stddef.h>

static volatile uint32_t crc_error;

static platform_status_t platform_crc_invalid(void) {
  platform_error_record(&crc_error, PLATFORM_ERROR_INVALID_ARGUMENT);
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

static platform_status_t platform_crc_finish(
  platform_status_t status
) {
  if (status != PLATFORM_STATUS_OK) {
    platform_error_record(&crc_error, platform_error_from_status(status));
  }
  return status;
}

platform_status_t Platform_CRC_Reset(platform_crc_instance_t crc) {
  platform_error_reset(&crc_error);
  if (!platform_crc_backend_is_valid(crc)) {
    return platform_crc_invalid();
  }
  return platform_crc_finish(platform_crc_backend_reset(crc));
}

platform_status_t Platform_CRC_AccumulateWords(
  platform_crc_instance_t crc,
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

platform_status_t Platform_CRC_CalculateWords(
  platform_crc_instance_t crc,
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

uint32_t Platform_CRC_GetLastError(platform_crc_instance_t crc) {
  return platform_crc_backend_is_valid(crc)
           ? crc_error
           : PLATFORM_ERROR_INVALID_ARGUMENT;
}
