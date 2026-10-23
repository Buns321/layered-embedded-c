/**
 * @file platform_crc.c
 * @brief STM32F1 HAL backend for the common CRC implementation.
 */

#include "crc.h"
#include "platform_crc_backend.h"
#include "stm32f1xx_hal.h"

static CRC_HandleTypeDef *platform_crc_to_hal(platform_crc_instance_t crc) {
  return crc == PLATFORM_CRC_1 ? &hcrc : NULL;
}

bool platform_crc_backend_is_valid(platform_crc_instance_t crc) {
  return platform_crc_to_hal(crc) != NULL;
}

platform_status_t platform_crc_backend_reset(
  platform_crc_instance_t crc
) {
  CRC_HandleTypeDef *crc_h = platform_crc_to_hal(crc);
  if (crc_h == NULL || crc_h->State != HAL_CRC_STATE_READY) {
    return crc_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT : PLATFORM_STATUS_ERROR;
  }
  __HAL_CRC_DR_RESET(crc_h);
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_crc_backend_accumulate(
  platform_crc_instance_t crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
) {
  CRC_HandleTypeDef *crc_h = platform_crc_to_hal(crc);
  if (crc_h == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (crc_h->State != HAL_CRC_STATE_READY) {
    return PLATFORM_STATUS_ERROR;
  }
  *result = HAL_CRC_Accumulate(crc_h, (uint32_t *)words, word_count);
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_crc_backend_calculate(
  platform_crc_instance_t crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
) {
  CRC_HandleTypeDef *crc_h = platform_crc_to_hal(crc);
  if (crc_h == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (crc_h->State != HAL_CRC_STATE_READY) {
    return PLATFORM_STATUS_ERROR;
  }
  *result = HAL_CRC_Calculate(crc_h, (uint32_t *)words, word_count);
  return PLATFORM_STATUS_OK;
}
