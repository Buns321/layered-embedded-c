/**
 * @file platform_flash_common.c
 * @brief 与芯片系列无关的 Flash 参数校验、忙状态和错误处理。
 * @ingroup platform
 */

#include "platform_error_internal.h"
#include "platform_flash_backend.h"
#include "system_critical.h"
#include <stddef.h>

static volatile bool flash_busy;
static uint32_t flash_error;

/**
 * @brief 获取 Flash 操作忙状态。
 */
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

/**
 * @brief 释放 Flash 操作忙状态。
 */
static void platform_flash_release(void) {
  Sys_Critical_Enter();
  flash_busy = false;
  Sys_Critical_Exit();
}

/**
 * @brief 记录 Flash 忙冲突。
 */
static void platform_flash_record_busy(void) {
  platform_error_record(&flash_error, PLATFORM_ERROR_BUSY);
}

/**
 * @brief 记录 Flash 后端失败。
 */
static void platform_flash_record_backend_failure(uint32_t operation_error) {
  uint32_t error = platform_flash_backend_get_error();
  if (error == PLATFORM_FLASH_ERROR_NONE) {
    error = PLATFORM_FLASH_ERROR_UNKNOWN;
  }
  platform_error_record(
    &flash_error,
    PLATFORM_ERROR_HAL | operation_error | error
  );
}

/**
 * @brief 从内部 Flash 复制字节。
 * @param instance 内部 Flash 实例。
 * @param address MCU Flash 地址空间中的起始地址。
 * @param data 目标缓冲区。
 * @param size 字节数，必须大于零。
 * @retval PLATFORM_STATUS_OK 成功。
 * @retval PLATFORM_STATUS_BUSY 另一个 Flash 操作正在进行。
 * @retval PLATFORM_STATUS_ERROR 地址范围非法或缓冲区为空。
 * @note 从任务上下文调用。应用中的所有 Flash 操作都必须使用这个 Platform
 *       实例，以确保忙状态保护具有唯一权威性。
 */
platform_status_t Platform_FLASH_Read(
  platform_flash_instance_t instance,
  uint32_t address,
  void *data,
  uint32_t size
) {
  platform_error_reset(&flash_error);
  if (
    instance != PLATFORM_FLASH_1 ||
    data == NULL ||
    !platform_flash_backend_range_valid(address, size)
  ) {
    platform_error_record(
      &flash_error,
      PLATFORM_ERROR_INVALID_ARGUMENT | PLATFORM_FLASH_ERROR_RANGE
    );
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (!platform_flash_acquire()) {
    platform_flash_record_busy();
    return PLATFORM_STATUS_BUSY;
  }

  platform_status_t status =
    platform_flash_backend_read(address, data, size);
  if (status != PLATFORM_STATUS_OK) {
    platform_flash_record_backend_failure(PLATFORM_FLASH_ERROR_PROGRAM);
  }
  platform_flash_release();
  return status;
}

/**
 * @brief 向已擦除的内部 Flash 写入 32 位字。
 * @param instance 内部 Flash 实例。
 * @param address 目标地址，必须按 4 字节对齐。
 * @param data 源缓冲区。
 * @param size 字节数，必须是四的非零整数倍。
 * @retval PLATFORM_STATUS_OK 成功。
 * @retval PLATFORM_STATUS_BUSY 另一个 Flash 操作正在进行。
 * @retval PLATFORM_STATUS_ERROR 地址范围或对齐非法，或 HAL 写入失败。
 * @note 目标区域必须已擦除，擦除策略由调用方负责。
 * @note 从任务上下文调用，并遵守目标 MCU 对中断或 RTOS 调度的要求。
 */
platform_status_t Platform_FLASH_Write(
  platform_flash_instance_t instance,
  uint32_t address,
  const void *data,
  uint32_t size
) {
  platform_error_reset(&flash_error);
  if (
    instance != PLATFORM_FLASH_1 ||
    data == NULL ||
    !platform_flash_backend_range_valid(address, size)
  ) {
    platform_error_record(
      &flash_error,
      PLATFORM_ERROR_INVALID_ARGUMENT | PLATFORM_FLASH_ERROR_RANGE
    );
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (((address & 3UL) != 0UL) || ((size & 3UL) != 0UL)) {
    platform_error_record(
      &flash_error,
      PLATFORM_ERROR_INVALID_ARGUMENT | PLATFORM_FLASH_ERROR_ALIGNMENT
    );
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (!platform_flash_acquire()) {
    platform_flash_record_busy();
    return PLATFORM_STATUS_BUSY;
  }

  platform_status_t status =
    platform_flash_backend_write(address, data, size);
  if (status != PLATFORM_STATUS_OK) {
    platform_flash_record_backend_failure(PLATFORM_FLASH_ERROR_PROGRAM);
  }

  platform_flash_release();
  return status;
}

/**
 * @brief 擦除与给定字节范围相交的所有 Flash 擦除单元。
 * @param instance 内部 Flash 实例。
 * @param address 起始地址，必须按擦除单元边界对齐。
 * @param size 字节数，必须大于零。
 * @retval PLATFORM_STATUS_OK 成功。
 * @retval PLATFORM_STATUS_BUSY 另一个 Flash 操作正在进行。
 * @retval PLATFORM_STATUS_ERROR 地址范围非法或 HAL 擦除失败。
 * @warning 与范围相交的所有页或扇区都会被擦除。选择参数槽位前应先调用
 *          Platform_FLASH_GetEraseUnitSize()。
 * @note 从任务上下文调用，且不能从正在被擦除的区域执行代码。
 */
platform_status_t Platform_FLASH_Erase(
  platform_flash_instance_t instance,
  uint32_t address,
  uint32_t size
) {
  platform_error_reset(&flash_error);
  if (
    instance != PLATFORM_FLASH_1 ||
    !platform_flash_backend_range_valid(address, size)
  ) {
    platform_error_record(
      &flash_error,
      PLATFORM_ERROR_INVALID_ARGUMENT | PLATFORM_FLASH_ERROR_RANGE
    );
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  const uint32_t erase_unit =
    platform_flash_backend_erase_unit_size(address);
  if (erase_unit == 0U || (address % erase_unit) != 0U) {
    platform_error_record(
      &flash_error,
      PLATFORM_ERROR_INVALID_ARGUMENT | PLATFORM_FLASH_ERROR_ALIGNMENT
    );
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (!platform_flash_acquire()) {
    platform_flash_record_busy();
    return PLATFORM_STATUS_BUSY;
  }

  platform_status_t status =
    platform_flash_backend_erase(address, size);
  if (status != PLATFORM_STATUS_OK) {
    platform_flash_record_backend_failure(PLATFORM_FLASH_ERROR_ERASE);
  }

  platform_flash_release();
  return status;
}

/**
 * @brief 获取覆盖指定地址的擦除单元大小。
 * @param instance 内部 Flash 实例。
 * @param address 待查询地址。
 * @return 擦除单元大小（字节）；地址不在 Flash 中时返回零。
 */
uint32_t Platform_FLASH_GetEraseUnitSize(
  platform_flash_instance_t instance,
  uint32_t address
) {
  if (instance != PLATFORM_FLASH_1) {
    return 0U;
  }
  return platform_flash_backend_erase_unit_size(address);
}

/**
 * @brief 读取最近一次 Platform Flash 错误快照。
 * @param instance 内部 Flash 实例。
 */
uint32_t Platform_FLASH_GetLastError(platform_flash_instance_t instance) {
  if (instance != PLATFORM_FLASH_1) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return flash_error;
}
