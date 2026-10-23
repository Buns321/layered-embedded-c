/**
 * @file platform_crc.c
 * @brief STM32F1 HAL backend for the common CRC implementation.
 */

#include "crc.h"
#include "platform_crc_backend.h"
#include "stm32f1xx_hal.h"

static CRC_HandleTypeDef *platform_crc_to_hal(Platform_CRC_Instance crc) {
  return crc == PLATFORM_CRC_1 ? &hcrc : NULL;
}

bool platform_crc_backend_is_valid(Platform_CRC_Instance crc) {
  return platform_crc_to_hal(crc) != NULL;
}

Platform_StatusTypeDef platform_crc_backend_reset(
  Platform_CRC_Instance crc
) {
  CRC_HandleTypeDef *h = platform_crc_to_hal(crc);
  if (h == NULL || h->State != HAL_CRC_STATE_READY) {
    return h == NULL ? Platform_INVALID_ARGUMENT : Platform_ERROR;
  }
  __HAL_CRC_DR_RESET(h);
  return Platform_OK;
}

Platform_StatusTypeDef platform_crc_backend_accumulate(
  Platform_CRC_Instance crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
) {
  CRC_HandleTypeDef *h = platform_crc_to_hal(crc);
  if (h == NULL) {
    return Platform_INVALID_ARGUMENT;
  }
  if (h->State != HAL_CRC_STATE_READY) {
    return Platform_ERROR;
  }
  *result = HAL_CRC_Accumulate(h, (uint32_t *)words, word_count);
  return Platform_OK;
}

Platform_StatusTypeDef platform_crc_backend_calculate(
  Platform_CRC_Instance crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
) {
  CRC_HandleTypeDef *h = platform_crc_to_hal(crc);
  if (h == NULL) {
    return Platform_INVALID_ARGUMENT;
  }
  if (h->State != HAL_CRC_STATE_READY) {
    return Platform_ERROR;
  }
  *result = HAL_CRC_Calculate(h, (uint32_t *)words, word_count);
  return Platform_OK;
}
