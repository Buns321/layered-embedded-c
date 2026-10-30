/**
 * @file platform_flash.c
 * @brief STM32F4 公共 Flash 实现的 HAL 后端。
 * @ingroup platform
 */

#include "platform_flash_backend.h"
#include "platform_hal_internal.h"
#include "stm32f4xx_hal.h"
#include <string.h>

#define PLATFORM_F4_FLASH_START FLASH_BASE
#define PLATFORM_F4_FLASH_END   (FLASH_BASE + 0x0007FFFFUL)

static uint32_t flash_backend_error;

/**
 * @brief 返回 F4 指定 Flash 扇区的起始字节地址。
 * @param sector FLASH_SECTOR_0 到 FLASH_SECTOR_7 之一；其他值不作为有效扇区。
 * @return 扇区起始物理地址；sector 无效时返回零。
 * @note 纯常量映射，不读取或修改 Flash；地址按本文件定义的 512 KiB 映射计算。
 */
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

/**
 * @brief 返回 F4 指定 Flash 扇区的大小。
 * @param sector FLASH_SECTOR_0 到 FLASH_SECTOR_7 之一；其他值不作为有效扇区。
 * @return 扇区字节数：扇区 0..3 为 16 KiB，扇区 4 为 64 KiB，扇区 5..7 为 128 KiB；
 *         sector 无效时返回零。
 * @note 纯常量映射，不读取或修改 Flash；可在任务或中断上下文调用。
 */
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

/**
 * @brief 查找包含给定地址的 F4 Flash 扇区。
 * @param address 内部 Flash 字节地址。
 * @return FLASH_SECTOR_0 到 FLASH_SECTOR_7 之一；地址不落在本文件定义的 512 KiB 范围时返回 UINT32_MAX。
 * @note 线性扫描固定扇区表，不访问目标 Flash 内容；可在任务或中断上下文调用。
 */
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

/**
 * @brief 把 F4 HAL Flash 错误位映射为平台错误位。
 * @param hal_error HAL_FLASH_ERROR_* 位掩码；编译期未定义的位不会被检查。
 * @return PLATFORM_FLASH_ERROR_* 位掩码；没有已知位置位时返回 PLATFORM_FLASH_ERROR_NONE。
 * @note 纯位映射，不读取寄存器、不修改快照；未知位对 NONE 的补足由 capture 辅助函数完成。
 */
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

/**
 * @brief 合并操作类别与 HAL 错误并覆盖 Flash 后端快照。
 * @param operation_error 本次操作对应的 PLATFORM_FLASH_ERROR_* 位，例如 PROGRAM 或 ERASE。
 * @post 静态快照等于 operation_error 与 HAL 映射位之并；没有已知 HAL 错误位时并入 UNKNOWN。
 * @note 只读取 HAL 错误寄存器并更新本编译单元的软件快照，不改变 Flash 内容或锁状态。
 * @warning 快照不是原子事务；只允许公共 Flash 层在串行化的任务上下文中调用，不得在 ISR 中使用。
 */
static void platform_flash_capture_error(uint32_t operation_error) {
  uint32_t error = platform_flash_map_hal_error(HAL_FLASH_GetError());
  if (error == PLATFORM_FLASH_ERROR_NONE) {
    error = PLATFORM_FLASH_ERROR_UNKNOWN;
  }
  flash_backend_error = operation_error | error;
}

/**
 * @brief 校验字节范围是否完整落在内部 Flash 地址空间内。
 * @param address 起始 Flash 字节地址；按芯片地址空间解释，本函数不检查对齐。
 * @param size 待访问字节数；必须大于零。
 * @retval true address 到 address + size - 1 的范围未越过该系列内部 Flash 末尾。
 * @retval false size 为零、address 低于 Flash 基址，或范围跨越 Flash 末尾。
 * @note 纯范围查询，不读取或修改 Flash；F1 运行期读取容量，F4 使用固定映射范围。
 * @note 可在任务或中断上下文调用，无共享状态修改。
 */
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

/**
 * @brief 返回覆盖指定地址的 Flash 擦除单元大小。
 * @param address 待查询的 Flash 字节地址；允许位于擦除单元内部。
 * @return 覆盖该地址的擦除单元字节数；F1 为页大小，F4 为 16 KiB、64 KiB 或 128 KiB；
 *         地址不在内部 Flash 中时返回零。
 * @note 纯查询，不擦除或修改 Flash；返回值可用于计算调用方必须遵守的起始地址对齐。
 * @note 可在任务或中断上下文调用，无共享状态修改。
 */
uint32_t platform_flash_backend_erase_unit_size(uint32_t address) {
  const uint32_t sector = platform_flash_find_sector(address);
  return sector == UINT32_MAX
           ? 0U
           : platform_flash_sector_size(sector);
}

/**
 * @brief 从内部 Flash 直接复制一段字节到调用方缓冲区。
 * @param address 源 Flash 字节地址；必须与 data、size 构成可访问且位于内部 Flash 的范围。
 * @param data 目标缓冲区；由调用方拥有，至少可写 size 字节，调用期间不得被其他执行流修改。
 * @param size 要复制的字节数；必须大于零。
 * @return 该后端固定返回 PLATFORM_STATUS_OK；本函数自身不进行范围或空指针校验。
 * @pre 公共 Flash 层已完成实例、范围、大小和缓冲区校验，并持有 Flash 忙状态。
 * @pre address 指向可直接读取的内部 Flash，data 非 NULL 且两者不重叠。
 * @post data 前 size 字节包含 address 起始处的 Flash 内容；错误快照和 HAL 状态不变。
 * @warning 非法地址、空指针或不可写目标会触发底层存储器异常；本函数无法把这类故障
 *          转换为平台错误码。读取不阻塞外设，可在任务上下文通过公共 Flash API 调用。
 */
platform_status_t platform_flash_backend_read(
  uint32_t address,
  void *data,
  uint32_t size
) {
  (void)memcpy(data, (const void *)address, size);
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 以 32 位字为单位编程一段已擦除的内部 Flash。
 * @param address 目标起始字节地址；必须按 4 字节对齐且范围位于内部 Flash 内。
 * @param data 源缓冲区；由调用方拥有，至少可读 size 字节，调用期间不得被其他执行流修改。
 * @param size 要写入的字节数；必须为 4 的非零整数倍。
 * @retval PLATFORM_STATUS_OK 所有目标字均编程成功，后端错误快照清零。
 * @retval PLATFORM_STATUS_ERROR 至少一个目标字编程失败；后续字不再写入，快照保留 PROGRAM 与 HAL 错误位。
 * @retval PLATFORM_STATUS_BUSY Flash HAL 报告忙时原样映射。
 * @retval PLATFORM_STATUS_TIMEOUT Flash HAL 报告超时时原样映射。
 * @pre 公共 Flash 层已完成范围、4 字节对齐、缓冲区和忙状态校验，并保证目标区域已擦除。
 * @post 成功时目标字具有 data 中的内容；失败时可能已有前若干字被编程，需由上层擦除后重试。
 * @warning 内部解锁 Flash 编程寄存器并逐字调用 HAL_FLASH_Program()，返回前重新上锁；
 *          不得从正在编程的 Flash 区域取指，且同一区域不得并发写或擦除。
 * @note 仅在任务上下文通过公共 Flash API 调用；函数不分配内存，也不在内部执行延时。
 */
platform_status_t platform_flash_backend_write(
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

/**
 * @brief 擦除与给定字节范围相交的所有 Flash 擦除单元。
 * @param address 起始 Flash 字节地址；公共层调用前要求按擦除单元边界对齐。
 * @param size 要覆盖的字节数；必须大于零，末尾不必与擦除单元边界对齐。
 * @retval PLATFORM_STATUS_OK 所有相交擦除单元均擦除成功，后端错误快照清零。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT F4 无法把范围映射到扇区；F1 后端不单独产生此状态。
 * @retval PLATFORM_STATUS_ERROR HAL 擦除失败或报告页/扇区错误；错误快照记录 ERASE 及映射错误位。
 * @retval PLATFORM_STATUS_BUSY Flash HAL 报告忙时原样映射。
 * @retval PLATFORM_STATUS_TIMEOUT Flash HAL 报告超时时原样映射。
 * @pre 公共 Flash 层已完成范围和对齐校验并持有 Flash 忙状态；不得与被擦除区域并发访问。
 * @post 相交页或扇区内容全部变为擦除态；失败时可能只有部分单元被擦除。
 * @warning F1 按页向上取整，F4 按首尾扇区整体擦除，因此范围外但相交的单元也会丢失；
 *          擦除期间不能从目标 Flash 区域执行代码。
 * @note 内部解锁 Flash 擦除寄存器并调用 HAL 擦除，返回前重新上锁；仅允许任务上下文调用。
 */
platform_status_t platform_flash_backend_erase(
  uint32_t address,
  uint32_t size
) {
  const uint32_t first_sector = platform_flash_find_sector(address);
  const uint32_t last_sector = platform_flash_find_sector(
    address + size - 1UL
  );
  if (first_sector == UINT32_MAX || last_sector == UINT32_MAX) {
    flash_backend_error = PLATFORM_FLASH_ERROR_RANGE;
    return PLATFORM_STATUS_INVALID_ARGUMENT;
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

/**
 * @brief 返回本编译单元保存的 Flash 后端错误快照。
 * @return PLATFORM_FLASH_ERROR_* 位掩码；PLATFORM_FLASH_ERROR_NONE 表示尚未记录错误，
 *         或最近一次后端写/擦除成功。
 * @note 只读且不清除快照；F1/F4 后端分别保存最近一次写或擦除产生的错误位。
 * @note 快照没有额外锁保护；调用方应在对应 Flash 操作结束后、同一任务上下文中读取。
 */
uint32_t platform_flash_backend_get_error(void) {
  return flash_backend_error;
}
