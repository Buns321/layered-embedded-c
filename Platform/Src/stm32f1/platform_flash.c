/**
 * @file platform_flash.c
 * @brief STM32F1 HAL backend for the common Flash implementation.
 */

#include "platform_flash_backend.h"
#include "platform_hal_internal.h"
#include "stm32f1xx_hal.h"
#include <string.h>

static uint32_t flash_backend_error;

static uint32_t platform_flash_size(void) {
  return (uint32_t)(*(volatile const uint16_t *)FLASH_SIZE_DATA_REGISTER) *
         1024UL;
}

bool platform_flash_backend_range_valid(uint32_t address, uint32_t size) {
  const uint32_t flash_size = platform_flash_size();
  if (
    flash_size == 0U ||
    size == 0U ||
    address < FLASH_BASE
  ) {
    return false;
  }

  const uint32_t offset = address - FLASH_BASE;
  return offset < flash_size && size <= flash_size - offset;
}

uint32_t platform_flash_backend_erase_unit_size(uint32_t address) {
  return platform_flash_backend_range_valid(address, 1U)
           ? FLASH_PAGE_SIZE
           : 0U;
}

Platform_StatusTypeDef platform_flash_backend_read(
  uint32_t address,
  void *data,
  uint32_t size
) {
  (void)memcpy(data, (const void *)address, size);
  return Platform_OK;
}

static uint32_t platform_flash_map_hal_error(uint32_t hal_error) {
  uint32_t error = PLATFORM_FLASH_ERROR_NONE;

  if ((hal_error & HAL_FLASH_ERROR_PROG) != 0U) {
    error |= PLATFORM_FLASH_ERROR_PROGRAM;
  }
  if ((hal_error & HAL_FLASH_ERROR_WRP) != 0U) {
    error |= PLATFORM_FLASH_ERROR_WRITE_PROTECT;
  }
  if ((hal_error & HAL_FLASH_ERROR_OPTV) != 0U) {
    error |= PLATFORM_FLASH_ERROR_INTERNAL;
  }
  return error;
}

static void platform_flash_capture_error(uint32_t operation_error) {
  uint32_t error = platform_flash_map_hal_error(HAL_FLASH_GetError());
  if (error == PLATFORM_FLASH_ERROR_NONE) {
    error = PLATFORM_FLASH_ERROR_UNKNOWN;
  }
  flash_backend_error = operation_error | error;
}

Platform_StatusTypeDef platform_flash_backend_write(
  uint32_t address,
  const void *data,
  uint32_t size
) {
  HAL_StatusTypeDef status = HAL_OK;
  const uint8_t *bytes = (const uint8_t *)data;

  HAL_FLASH_Unlock();
  for (uint32_t offset = 0U; offset < size; offset += 4UL) {
    uint32_t word;
    (void)memcpy(&word, bytes + offset, sizeof(word));
    status = HAL_FLASH_Program(
      FLASH_TYPEPROGRAM_WORD,
      address + offset,
      word
    );
    if (status != HAL_OK) {
      break;
    }
  }
  HAL_FLASH_Lock();

  if (status != HAL_OK) {
    platform_flash_capture_error(PLATFORM_FLASH_ERROR_PROGRAM);
  } else {
    flash_backend_error = PLATFORM_FLASH_ERROR_NONE;
  }
  return platform_from_hal_status(status);
}

Platform_StatusTypeDef platform_flash_backend_erase(
  uint32_t address,
  uint32_t size
) {
  const uint32_t page_count =
    (size + FLASH_PAGE_SIZE - 1UL) / FLASH_PAGE_SIZE;
  FLASH_EraseInitTypeDef erase = {
    .TypeErase = FLASH_TYPEERASE_PAGES,
    .PageAddress = address,
    .NbPages = page_count,
  };
  uint32_t page_error = 0U;

  HAL_FLASH_Unlock();
  HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase, &page_error);
  HAL_FLASH_Lock();

  if (status != HAL_OK || page_error != UINT32_MAX) {
    platform_flash_capture_error(PLATFORM_FLASH_ERROR_ERASE);
    if (page_error != UINT32_MAX) {
      flash_backend_error |= PLATFORM_FLASH_ERROR_INTERNAL;
    }
    status = HAL_ERROR;
  } else {
    flash_backend_error = PLATFORM_FLASH_ERROR_NONE;
  }
  return platform_from_hal_status(status);
}

uint32_t platform_flash_backend_get_error(void) {
  return flash_backend_error;
}
