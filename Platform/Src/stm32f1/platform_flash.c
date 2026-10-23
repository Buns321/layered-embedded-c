//
// 由 Buns_ 创建于 2026/10/23。
//

#include "platform_flash.h"
#include "platform_hal_internal.h"
#include "stm32f1xx_hal.h"
#include "system_critical.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>

static volatile bool flash_busy;
static uint32_t flash_error = PLATFORM_FLASH_ERROR_NONE;

static uint32_t platform_flash_size(void) {
  return (uint32_t)(*(volatile const uint16_t *)FLASH_SIZE_DATA_REGISTER) *
         1024UL;
}

static bool platform_flash_acquire(void) {
  bool acquired = false;
  Sys_Critical_Enter();
  if (!flash_busy) {
    flash_busy = true;
    acquired = true;
  }
  Sys_Critical_Exit();
  return acquired;
}

static void platform_flash_release(void) {
  Sys_Critical_Enter();
  flash_busy = false;
  Sys_Critical_Exit();
}

static bool platform_flash_range_valid(uint32_t address, uint32_t size) {
  uint32_t flash_size = platform_flash_size();
  if (flash_size == 0U || size == 0U || address < FLASH_BASE) {
    return false;
  }
  uint32_t offset = address - FLASH_BASE;
  return offset < flash_size && size <= flash_size - offset;
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
  uint32_t mapped = platform_flash_map_hal_error(HAL_FLASH_GetError());
  if (mapped == PLATFORM_FLASH_ERROR_NONE) {
    mapped = PLATFORM_FLASH_ERROR_UNKNOWN;
  }
  flash_error = operation_error | mapped;
}

Platform_StatusTypeDef Platform_FLASH_Read(
  Platform_FLASH_Instance instance,
  uint32_t address,
  void *data,
  uint32_t size
) {
  assert(instance == PLATFORM_FLASH_1);
  (void)instance;
  if (data == NULL || !platform_flash_range_valid(address, size)) {
    flash_error = PLATFORM_FLASH_ERROR_RANGE;
    return Platform_ERROR;
  }
  if (!platform_flash_acquire()) {
    return Platform_BUSY;
  }

  (void)memcpy(data, (const void *)address, size);
  flash_error = PLATFORM_FLASH_ERROR_NONE;
  platform_flash_release();
  return Platform_OK;
}

Platform_StatusTypeDef Platform_FLASH_Write(
  Platform_FLASH_Instance instance,
  uint32_t address,
  const void *data,
  uint32_t size
) {
  assert(instance == PLATFORM_FLASH_1);
  (void)instance;
  if (data == NULL || !platform_flash_range_valid(address, size)) {
    flash_error = PLATFORM_FLASH_ERROR_RANGE;
    return Platform_ERROR;
  }
  if (((address & 3UL) != 0UL) || ((size & 3UL) != 0UL)) {
    flash_error = PLATFORM_FLASH_ERROR_ALIGNMENT;
    return Platform_ERROR;
  }
  if (!platform_flash_acquire()) {
    return Platform_BUSY;
  }

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
    flash_error = PLATFORM_FLASH_ERROR_NONE;
  }

  platform_flash_release();
  return platform_from_hal_status(status);
}

Platform_StatusTypeDef Platform_FLASH_Erase(
  Platform_FLASH_Instance instance,
  uint32_t address,
  uint32_t size
) {
  assert(instance == PLATFORM_FLASH_1);
  (void)instance;
  if (!platform_flash_range_valid(address, size)) {
    flash_error = PLATFORM_FLASH_ERROR_RANGE;
    return Platform_ERROR;
  }
  if ((address % FLASH_PAGE_SIZE) != 0U) {
    flash_error = PLATFORM_FLASH_ERROR_ALIGNMENT;
    return Platform_ERROR;
  }
  if (!platform_flash_acquire()) {
    return Platform_BUSY;
  }

  uint32_t page_address = address;
  uint32_t page_count =
    (size + FLASH_PAGE_SIZE - 1UL) / FLASH_PAGE_SIZE;
  FLASH_EraseInitTypeDef erase = {
    .TypeErase = FLASH_TYPEERASE_PAGES,
    .PageAddress = page_address,
    .NbPages = page_count,
  };
  uint32_t page_error = 0U;

  HAL_FLASH_Unlock();
  HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase, &page_error);
  HAL_FLASH_Lock();

  if (status != HAL_OK || page_error != UINT32_MAX) {
    platform_flash_capture_error(PLATFORM_FLASH_ERROR_ERASE);
    if (page_error != UINT32_MAX) {
      flash_error |= PLATFORM_FLASH_ERROR_INTERNAL;
    }
    status = HAL_ERROR;
  } else {
    flash_error = PLATFORM_FLASH_ERROR_NONE;
  }

  platform_flash_release();
  return platform_from_hal_status(status);
}

uint32_t Platform_FLASH_GetEraseUnitSize(
  Platform_FLASH_Instance instance,
  uint32_t address
) {
  assert(instance == PLATFORM_FLASH_1);
  (void)instance;
  return platform_flash_range_valid(address, 1U) ? FLASH_PAGE_SIZE : 0U;
}

uint32_t Platform_FLASH_GetLastError(Platform_FLASH_Instance instance) {
  assert(instance == PLATFORM_FLASH_1);
  (void)instance;
  return flash_error;
}
