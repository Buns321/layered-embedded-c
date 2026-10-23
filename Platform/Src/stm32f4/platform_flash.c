/**
 * @file platform_flash.c
 * @brief STM32F4 HAL backend for the common Flash implementation.
 */

#include "platform_flash_backend.h"
#include "platform_hal_internal.h"
#include "stm32f4xx_hal.h"
#include <string.h>

#define PLATFORM_F4_FLASH_START FLASH_BASE
#define PLATFORM_F4_FLASH_END   (FLASH_BASE + 0x0007FFFFUL)

static uint32_t flash_backend_error;

static uint32_t platform_flash_sector_start(uint32_t sector) {
  switch (sector) {
    case FLASH_SECTOR_0: return FLASH_BASE;
    case FLASH_SECTOR_1: return FLASH_BASE + 0x00004000UL;
    case FLASH_SECTOR_2: return FLASH_BASE + 0x00008000UL;
    case FLASH_SECTOR_3: return FLASH_BASE + 0x0000C000UL;
    case FLASH_SECTOR_4: return FLASH_BASE + 0x00010000UL;
    case FLASH_SECTOR_5: return FLASH_BASE + 0x00020000UL;
    case FLASH_SECTOR_6: return FLASH_BASE + 0x00040000UL;
    case FLASH_SECTOR_7: return FLASH_BASE + 0x00060000UL;
    default: return 0U;
  }
}

static uint32_t platform_flash_sector_size(uint32_t sector) {
  switch (sector) {
    case FLASH_SECTOR_0:
    case FLASH_SECTOR_1:
    case FLASH_SECTOR_2:
    case FLASH_SECTOR_3:
      return 0x00004000UL;
    case FLASH_SECTOR_4:
      return 0x00010000UL;
    case FLASH_SECTOR_5:
    case FLASH_SECTOR_6:
    case FLASH_SECTOR_7:
      return 0x00020000UL;
    default:
      return 0U;
  }
}

static uint32_t platform_flash_find_sector(uint32_t address) {
  if (
    address < PLATFORM_F4_FLASH_START ||
    address > PLATFORM_F4_FLASH_END
  ) {
    return UINT32_MAX;
  }

  for (uint32_t sector = FLASH_SECTOR_0; sector <= FLASH_SECTOR_7;
       ++sector) {
    const uint32_t start = platform_flash_sector_start(sector);
    const uint32_t size = platform_flash_sector_size(sector);
    if (address >= start && address < start + size) {
      return sector;
    }
  }
  return UINT32_MAX;
}

static uint32_t platform_flash_map_hal_error(uint32_t hal_error) {
  uint32_t error = PLATFORM_FLASH_ERROR_NONE;

#ifdef HAL_FLASH_ERROR_RD
  if ((hal_error & HAL_FLASH_ERROR_RD) != 0U) {
    error |= PLATFORM_FLASH_ERROR_VERIFY;
  }
#endif
#ifdef HAL_FLASH_ERROR_PGS
  if ((hal_error & HAL_FLASH_ERROR_PGS) != 0U) {
    error |= PLATFORM_FLASH_ERROR_PROGRAM;
  }
#endif
#ifdef HAL_FLASH_ERROR_PGP
  if ((hal_error & HAL_FLASH_ERROR_PGP) != 0U) {
    error |= PLATFORM_FLASH_ERROR_PROGRAM;
  }
#endif
#ifdef HAL_FLASH_ERROR_PGA
  if ((hal_error & HAL_FLASH_ERROR_PGA) != 0U) {
    error |= PLATFORM_FLASH_ERROR_ALIGNMENT;
  }
#endif
#ifdef HAL_FLASH_ERROR_WRP
  if ((hal_error & HAL_FLASH_ERROR_WRP) != 0U) {
    error |= PLATFORM_FLASH_ERROR_WRITE_PROTECT;
  }
#endif
#ifdef HAL_FLASH_ERROR_OPERATION
  if ((hal_error & HAL_FLASH_ERROR_OPERATION) != 0U) {
    error |= PLATFORM_FLASH_ERROR_INTERNAL;
  }
#endif
#ifdef HAL_FLASH_ERROR_PROG
  if ((hal_error & HAL_FLASH_ERROR_PROG) != 0U) {
    error |= PLATFORM_FLASH_ERROR_PROGRAM;
  }
#endif

  return error;
}

static void platform_flash_capture_error(uint32_t operation_error) {
  uint32_t error = platform_flash_map_hal_error(HAL_FLASH_GetError());
  if (error == PLATFORM_FLASH_ERROR_NONE) {
    error = PLATFORM_FLASH_ERROR_UNKNOWN;
  }
  flash_backend_error = operation_error | error;
}

bool platform_flash_backend_range_valid(uint32_t address, uint32_t size) {
  if (
    size == 0U ||
    address < PLATFORM_F4_FLASH_START ||
    address > PLATFORM_F4_FLASH_END
  ) {
    return false;
  }
  return size <= (PLATFORM_F4_FLASH_END - address + 1UL);
}

uint32_t platform_flash_backend_erase_unit_size(uint32_t address) {
  const uint32_t sector = platform_flash_find_sector(address);
  return sector == UINT32_MAX
           ? 0U
           : platform_flash_sector_size(sector);
}

Platform_StatusTypeDef platform_flash_backend_read(
  uint32_t address,
  void *data,
  uint32_t size
) {
  (void)memcpy(data, (const void *)address, size);
  return Platform_OK;
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
  const uint32_t first_sector = platform_flash_find_sector(address);
  const uint32_t last_sector = platform_flash_find_sector(
    address + size - 1UL
  );
  if (first_sector == UINT32_MAX || last_sector == UINT32_MAX) {
    flash_backend_error = PLATFORM_FLASH_ERROR_RANGE;
    return Platform_INVALID_ARGUMENT;
  }

  FLASH_EraseInitTypeDef erase = {0};
  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.Banks = FLASH_BANK_1;
  erase.Sector = first_sector;
  erase.NbSectors = last_sector - first_sector + 1UL;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

  uint32_t sector_error = 0U;
  HAL_FLASH_Unlock();
  HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase, &sector_error);
  HAL_FLASH_Lock();

  if (status != HAL_OK || sector_error != UINT32_MAX) {
    platform_flash_capture_error(PLATFORM_FLASH_ERROR_ERASE);
    if (sector_error != UINT32_MAX) {
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
