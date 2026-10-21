//
// Created by Buns_ on 2026/10/22.
//

#include "platform_crc.h"
#include "crc.h"
#include "stm32f4xx_hal.h"
#include <assert.h>

static CRC_HandleTypeDef *platform_crc_to_hal(Platform_CRC_Instance crc) {
  assert(crc == PLATFORM_CRC_1);
  if (crc != PLATFORM_CRC_1) {
    return NULL;
  }

  assert(hcrc.State == HAL_CRC_STATE_READY);
  return &hcrc;
}

void Platform_CRC_Reset(Platform_CRC_Instance crc) {
  CRC_HandleTypeDef *handle = platform_crc_to_hal(crc);
  assert(handle != NULL);
  __HAL_CRC_DR_RESET(handle);
}

uint32_t Platform_CRC_AccumulateWords(
  Platform_CRC_Instance crc,
  const uint32_t *words,
  uint32_t word_count
) {
  assert(words != NULL && word_count != 0U);

  CRC_HandleTypeDef *handle = platform_crc_to_hal(crc);
  assert(handle != NULL);

  return HAL_CRC_Accumulate(
    handle,
    (uint32_t *)words,
    word_count
  );
}

uint32_t Platform_CRC_CalculateWords(
  Platform_CRC_Instance crc,
  const uint32_t *words,
  uint32_t word_count
) {
  assert(words != NULL && word_count != 0U);

  CRC_HandleTypeDef *handle = platform_crc_to_hal(crc);
  assert(handle != NULL);

  return HAL_CRC_Calculate(
    handle,
    (uint32_t *)words,
    word_count
  );
}
