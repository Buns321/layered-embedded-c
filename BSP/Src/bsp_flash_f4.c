/**
 * @file bsp_flash_f4.c
 * @brief STM32F4 内部 Flash 读写擦除后端（按 HAL_FLASH_MODULE_ENABLED 启用）。
 * @details 适配 STM32F407VET6（512 KiB，Sector 0..7 非均匀扇区布局），
 *          通过临界区互斥标志保护并发访问，并严格校验 4 字节写入对齐与扇区边界擦除对齐。
 * @ingroup BSP
 */

#include "bsp_flash.h"
#include "bsp_mcu.h"

#if defined(HAL_FLASH_MODULE_ENABLED) && defined(STM32F4xx)

#include "bsp_hal_internal.h"
#include "sys_critical.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/** @brief STM32F407VET6 片上主 Flash 起始物理地址（512 KiB 容量起址）。 */
#define BSP_F4_FLASH_START FLASH_BASE
/** @brief STM32F407VET6 片上主 Flash 末字节物理地址（`FLASH_BASE + 512 KiB - 1`）。 */
#define BSP_F4_FLASH_END (FLASH_BASE + 0x0007FFFFUL)

/** @brief Flash 读写擦除操作互斥忙标志（由临界区保护原子获取与释放）。 */
static volatile bool s_flash_busy;

/**
 * @brief 尝试原子获取 Flash 操作互斥锁。
 * @return 是否成功获取访问权。
 * @retval true  获取成功，调用方须在操作完成后调用 bsp_flash_release()。
 * @retval false 当前已有其他任务正在访问 Flash。
 */
static bool bsp_flash_acquire(void) {
  bool acquired = false;
  // 关中断临界区保护 test-and-set，防止多任务同时解锁/编程 Flash 控制器
  Sys_Critical_Enter();
  if (!s_flash_busy) {
    s_flash_busy = true;
    acquired = true;
  }
  Sys_Critical_Exit();
  return acquired;
}

/**
 * @brief 释放 Flash 操作互斥锁。
 */
static void bsp_flash_release(void) {
  Sys_Critical_Enter();
  s_flash_busy = false;
  Sys_Critical_Exit();
}

/**
 * @brief 查询 STM32F407VET6 指定扇区编号的起始物理地址。
 * @param[in] sector HAL 扇区编号（`FLASH_SECTOR_0..FLASH_SECTOR_7`）。
 * @return 扇区起始地址（单位：byte）；非法扇区编号返回 0。
 */
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

/**
 * @brief 查询 STM32F407VET6 指定扇区编号的字节容量。
 * @param[in] sector HAL 扇区编号（`FLASH_SECTOR_0..FLASH_SECTOR_7`）。
 * @return 扇区大小（单位：byte，16 KiB / 64 KiB / 128 KiB）；非法扇区返回 0。
 */
static uint32_t bsp_flash_sectorsize(uint32_t sector) {
  // F407 扇区非均匀分布：Sector 0..3 为 16 KiB，Sector 4 为 64 KiB，Sector 5..7 为 128 KiB
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

/**
 * @brief 根据物理地址反查所属的 Flash 扇区编号。
 * @param[in] addr 片上 Flash 字节地址。
 * @return 扇区编号（`FLASH_SECTOR_0..FLASH_SECTOR_7`）；地址越界时返回 `UINT32_MAX`。
 */
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

/**
 * @brief 校验 `[addr, addr + size_bytes)` 是否完全位于合法片上 Flash 范围内。
 * @param[in] addr       起始字节地址。
 * @param[in] size_bytes 区间长度（单位：byte）。
 * @return 区间是否合法（无零长度且无 32 位地址加法溢出）。
 */
static bool bsp_flash_rangevalid(uint32_t addr, uint32_t size_bytes) {
  if (size_bytes == 0U || addr < BSP_F4_FLASH_START || addr > BSP_F4_FLASH_END) {
    return false;
  }
  // 使用减法比较剩余空间，避免 addr + size_bytes 发生 32 位无符号溢出
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

  // STM32F4 片上 Flash 线性映射到地址空间，可直接通过 memcpy 读取
  (void)memcpy(out, (const void *)(uintptr_t)addr, size_bytes);
  bsp_flash_release();
  return BSP_OK;
}

bsp_status_t BSP_Flash_Write(uint32_t addr, const void *data, uint32_t size_bytes) {
  // F4 3.3V 供电下按 32 位字编程效率最高且契合硬件对齐要求，强制校验 4 字节对齐
  if (data == NULL || !bsp_flash_rangevalid(addr, size_bytes) || ((addr & 3UL) != 0UL) ||
      ((size_bytes & 3UL) != 0UL)) {
    return BSP_ERR_INVALID_ARG;
  }
  if (!bsp_flash_acquire()) {
    return BSP_ERR_BUSY;
  }

  HAL_StatusTypeDef hal_st = HAL_OK;
  const uint8_t *bytes = (const uint8_t *)data;
  // 解锁 FLASH_CR 控制寄存器后逐字编程，无论成功或失败均须重新上锁保护
  (void)HAL_FLASH_Unlock();
  for (uint32_t offset = 0U; offset < size_bytes; offset += 4UL) {
    uint32_t word = 0U;
    // 通过 memcpy 组装 32 位字，避免调用方传入非对齐 data 指针触发硬件对齐异常
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

  // 擦除起始地址必须严格对齐到所在扇区的起始边界，防止误以为仅擦除部分扇区
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

  // F407 逻辑供电为 3.3V，采用 FLASH_VOLTAGE_RANGE_3（2.7V..3.6V，32 位并行擦写）
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
  // HAL 约定仅当所有扇区全部擦除成功时 sector_error 才保持 0xFFFFFFFF
  return (sector_error == 0xFFFFFFFFU) ? BSP_OK : BSP_ERR_HAL;
}

#else

/** @brief 未启用 HAL_FLASH_MODULE_ENABLED 或非 STM32F4xx 时的 ISO C11 非空翻译单元占位类型。 */
typedef int bsp_flash_f4_empty_tu_t;

#endif /* HAL_FLASH_MODULE_ENABLED && STM32F4xx */
