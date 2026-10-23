/**
 * @file platform_flash_common.c
 * @brief Family-independent Flash validation, busy state and error handling.
 */

#include "platform_error_internal.h"
#include "platform_flash_backend.h"
#include "system_critical.h"
#include <stddef.h>

static volatile bool flash_busy;
static uint32_t flash_error;

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

static void platform_flash_record_busy(void) {
  platform_error_record(&flash_error, PLATFORM_COMMON_ERROR_BUSY);
}

static void platform_flash_record_backend_failure(uint32_t operation_error) {
  uint32_t error = platform_flash_backend_get_error();
  if (error == PLATFORM_FLASH_ERROR_NONE) {
    error = PLATFORM_FLASH_ERROR_UNKNOWN;
  }
  platform_error_record(
    &flash_error,
    PLATFORM_COMMON_ERROR_HAL | operation_error | error
  );
}

Platform_StatusTypeDef Platform_FLASH_Read(
  Platform_FLASH_Instance instance,
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
      PLATFORM_COMMON_ERROR_PARAMETER | PLATFORM_FLASH_ERROR_RANGE
    );
    return Platform_INVALID_ARGUMENT;
  }
  if (!platform_flash_acquire()) {
    platform_flash_record_busy();
    return Platform_BUSY;
  }

  Platform_StatusTypeDef status =
    platform_flash_backend_read(address, data, size);
  if (status != Platform_OK) {
    platform_flash_record_backend_failure(PLATFORM_FLASH_ERROR_PROGRAM);
  }
  platform_flash_release();
  return status;
}

Platform_StatusTypeDef Platform_FLASH_Write(
  Platform_FLASH_Instance instance,
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
      PLATFORM_COMMON_ERROR_PARAMETER | PLATFORM_FLASH_ERROR_RANGE
    );
    return Platform_INVALID_ARGUMENT;
  }
  if (((address & 3UL) != 0UL) || ((size & 3UL) != 0UL)) {
    platform_error_record(
      &flash_error,
      PLATFORM_COMMON_ERROR_PARAMETER | PLATFORM_FLASH_ERROR_ALIGNMENT
    );
    return Platform_INVALID_ARGUMENT;
  }
  if (!platform_flash_acquire()) {
    platform_flash_record_busy();
    return Platform_BUSY;
  }

  Platform_StatusTypeDef status =
    platform_flash_backend_write(address, data, size);
  if (status != Platform_OK) {
    platform_flash_record_backend_failure(PLATFORM_FLASH_ERROR_PROGRAM);
  }

  platform_flash_release();
  return status;
}

Platform_StatusTypeDef Platform_FLASH_Erase(
  Platform_FLASH_Instance instance,
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
      PLATFORM_COMMON_ERROR_PARAMETER | PLATFORM_FLASH_ERROR_RANGE
    );
    return Platform_INVALID_ARGUMENT;
  }

  const uint32_t erase_unit =
    platform_flash_backend_erase_unit_size(address);
  if (erase_unit == 0U || (address % erase_unit) != 0U) {
    platform_error_record(
      &flash_error,
      PLATFORM_COMMON_ERROR_PARAMETER | PLATFORM_FLASH_ERROR_ALIGNMENT
    );
    return Platform_INVALID_ARGUMENT;
  }
  if (!platform_flash_acquire()) {
    platform_flash_record_busy();
    return Platform_BUSY;
  }

  Platform_StatusTypeDef status =
    platform_flash_backend_erase(address, size);
  if (status != Platform_OK) {
    platform_flash_record_backend_failure(PLATFORM_FLASH_ERROR_ERASE);
  }

  platform_flash_release();
  return status;
}

uint32_t Platform_FLASH_GetEraseUnitSize(
  Platform_FLASH_Instance instance,
  uint32_t address
) {
  if (instance != PLATFORM_FLASH_1) {
    return 0U;
  }
  return platform_flash_backend_erase_unit_size(address);
}

uint32_t Platform_FLASH_GetLastError(Platform_FLASH_Instance instance) {
  if (instance != PLATFORM_FLASH_1) {
    return PLATFORM_COMMON_ERROR_PARAMETER;
  }
  return flash_error;
}
