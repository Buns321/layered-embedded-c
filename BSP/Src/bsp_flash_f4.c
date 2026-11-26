/**
 * @file bsp_flash_f4.c
 * @brief STM32F4 内部 Flash 读写擦除后端（按 HAL_FLASH_MODULE_ENABLED 启用）。
 * @ingroup bsp
 */

#include "bsp_flash.h"
#include "bsp_mcu.h"

#if defined(HAL_FLASH_MODULE_ENABLED) && defined(STM32F4xx)

#include "bsp_hal_internal.h"
#include "sys_critical.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define BSP_F4_FLASH_START FLASH_BASE
#define BSP_F4_FLASH_END (FLASH_BASE + 0x0007FFFFUL)

static volatile bool s_flash_busy;

static bool bsp_flash_acquire(void) {
  bool acquired = false;
  Sys_Critical_Enter();
  if (!s_flash_busy) {
    s_flash_busy = true;
    acquired = true;
  }
  Sys_Critical_Exit();
  return acquired;
}

static void bsp_flash_release(void) {
  Sys_Critical_Enter();
  s_flash_busy = false;
  Sys_Critical_Exit();
}

static uint32_t bsp_flash_sectorstart(uint32_t sector) {
  switch (sector) {
  case FLASH_SECTOR_0:
    return FLASH_BASE;
  case FLASH_SECTOR_1:
    return FLASH_BASE + 0x00004000UL;
  case FLASH_SECTOR_2:
    return FLASH_BASE + 0x00008000UL;
  case FLASH_SECTOR_3:
    return FLASH_BASE + 0x0000C000UL;
  case FLASH_SECTOR_4:
    return FLASH_BASE + 0x00010000UL;
  case FLASH_SECTOR_5:
    return FLASH_BASE + 0x00020000UL;
  case FLASH_SECTOR_6:
    return FLASH_BASE + 0x00040000UL;
  case FLASH_SECTOR_7:
    return FLASH_BASE + 0x00060000UL;
  default:
    return 0U;
  }
}

static uint32_t bsp_flash_sectorsize(uint32_t sector) {
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

static uint32_t bsp_flash_findsector(uint32_t addr) {
  if (addr < BSP_F4_FLASH_START || addr > BSP_F4_FLASH_END) {
    return UINT32_MAX;
  }
  for (uint32_t sector = FLASH_SECTOR_0; sector <= FLASH_SECTOR_7; ++sector) {
    uint32_t start = bsp_flash_sectorstart(sector);
    uint32_t size = bsp_flash_sectorsize(sector);
    if (addr >= start && addr < start + size) {
      return sector;
    }
  }
  return UINT32_MAX;
}

static bool bsp_flash_rangevalid(uint32_t addr, uint32_t size_bytes) {
  if (size_bytes == 0U || addr < BSP_F4_FLASH_START || addr > BSP_F4_FLASH_END) {
    return false;
  }
  return size_bytes <= (BSP_F4_FLASH_END - addr + 1UL);
}

uint32_t BSP_Flash_GetEraseUnitSize(uint32_t addr) {
  uint32_t sector = bsp_flash_findsector(addr);
  return (sector == UINT32_MAX) ? 0U : bsp_flash_sectorsize(sector);
}

bsp_status_t BSP_Flash_Read(uint32_t addr, void *out, uint32_t size_bytes) {
  if (out == NULL || !bsp_flash_rangevalid(addr, size_bytes)) {
    return BSP_ERR_INVALID_ARG;
  }
  if (!bsp_flash_acquire()) {
    return BSP_ERR_BUSY;
  }

  (void)memcpy(out, (const void *)(uintptr_t)addr, size_bytes);
  bsp_flash_release();
  return BSP_OK;
}

bsp_status_t BSP_Flash_Write(uint32_t addr, const void *data, uint32_t size_bytes) {
  if (data == NULL || !bsp_flash_rangevalid(addr, size_bytes) || ((addr & 3UL) != 0UL) ||
      ((size_bytes & 3UL) != 0UL)) {
    return BSP_ERR_INVALID_ARG;
  }
  if (!bsp_flash_acquire()) {
    return BSP_ERR_BUSY;
  }

  HAL_StatusTypeDef hal_st = HAL_OK;
  const uint8_t *bytes = (const uint8_t *)data;
  (void)HAL_FLASH_Unlock();
  for (uint32_t offset = 0U; offset < size_bytes; offset += 4UL) {
    uint32_t word = 0U;
    (void)memcpy(&word, bytes + offset, sizeof(word));
    hal_st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + offset, word);
    if (hal_st != HAL_OK) {
      break;
    }
  }
  (void)HAL_FLASH_Lock();

  bsp_flash_release();
  return bsp_status_from_hal(hal_st);
}

bsp_status_t BSP_Flash_Erase(uint32_t start_addr, uint32_t size_bytes) {
  if (!bsp_flash_rangevalid(start_addr, size_bytes)) {
    return BSP_ERR_INVALID_ARG;
  }

  uint32_t erase_unit = BSP_Flash_GetEraseUnitSize(start_addr);
  if (erase_unit == 0U || (start_addr % erase_unit) != 0U) {
    return BSP_ERR_INVALID_ARG;
  }

  uint32_t first_sector = bsp_flash_findsector(start_addr);
  uint32_t last_sector = bsp_flash_findsector(start_addr + size_bytes - 1UL);
  if (first_sector == UINT32_MAX || last_sector == UINT32_MAX) {
    return BSP_ERR_INVALID_ARG;
  }

  if (!bsp_flash_acquire()) {
    return BSP_ERR_BUSY;
  }

  FLASH_EraseInitTypeDef erase = {0};
  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.Banks = FLASH_BANK_1;
  erase.Sector = first_sector;
  erase.NbSectors = last_sector - first_sector + 1UL;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

  uint32_t sector_error = 0U;
  (void)HAL_FLASH_Unlock();
  HAL_StatusTypeDef hal_st = HAL_FLASHEx_Erase(&erase, &sector_error);
  (void)HAL_FLASH_Lock();

  bsp_flash_release();
  if (hal_st != HAL_OK) {
    return bsp_status_from_hal(hal_st);
  }
  return (sector_error == 0xFFFFFFFFU) ? BSP_OK : BSP_ERR_HAL;
}

#else

typedef int bsp_flash_f4_empty_tu_t;

#endif /* HAL_FLASH_MODULE_ENABLED && STM32F4xx */
