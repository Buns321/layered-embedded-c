//
// Created by Buns_ on 2026/10/23.
//

/**
 * @file platform_flash.h
 * @brief HAL-free internal flash primitives.
 * @addtogroup platform
 * @{
 * @note This module owns only low-level read, program, and erase operations.
 *       Parameter layout, CRC validation, wear policy, and default-value
 *       fallback belong to the System parameter-storage layer.
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_FLASH_H
#define LAYERED_EMBEDDED_C_PLATFORM_FLASH_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

#if !PLATFORM_ENABLE_FLASH
#error "Platform flash must be enabled before including platform_flash.h"
#endif

/** @brief Available internal flash instance. */
typedef enum {
  PLATFORM_FLASH_1 = 0U,
} Platform_FLASH_Instance;

/** @brief Internal flash error flags. */
typedef enum {
  PLATFORM_FLASH_ERROR_NONE          = 0u,
  PLATFORM_FLASH_ERROR_PROGRAM       = (1u << 0),
  PLATFORM_FLASH_ERROR_ERASE         = (1u << 1),
  PLATFORM_FLASH_ERROR_WRITE_PROTECT = (1u << 2),
  PLATFORM_FLASH_ERROR_VERIFY        = (1u << 3),
  PLATFORM_FLASH_ERROR_ALIGNMENT     = (1u << 4),
  PLATFORM_FLASH_ERROR_RANGE         = (1u << 5),
  PLATFORM_FLASH_ERROR_INTERNAL      = (1u << 6),
  PLATFORM_FLASH_ERROR_UNKNOWN       = (1u << 7),
} Platform_FLASH_Error;

/**
 * @brief Copy bytes from internal flash.
 * @param instance Internal flash instance.
 * @param address Start address in the MCU flash address space.
 * @param data Destination buffer.
 * @param size Byte count, greater than zero.
 * @retval Platform_OK Success.
 * @retval Platform_BUSY Another flash operation is active.
 * @retval Platform_ERROR Invalid range or null buffer.
 * @note Call from task context. All flash operations in the application must
 *       use this Platform instance so the busy guard is authoritative.
 */
Platform_StatusTypeDef Platform_FLASH_Read(
  Platform_FLASH_Instance instance,
  uint32_t address,
  void *data,
  uint32_t size
);

/**
 * @brief Program 32-bit words into erased internal flash.
 * @param instance Internal flash instance.
 * @param address Destination address. Must be 4-byte aligned.
 * @param data Source buffer.
 * @param size Byte count. Must be a nonzero multiple of four.
 * @retval Platform_OK Success.
 * @retval Platform_BUSY Another flash operation is active.
 * @retval Platform_ERROR Invalid range, alignment, or HAL program failure.
 * @note Destination must already be erased. The caller owns erase policy.
 * @note Call from task context while interrupts or RTOS scheduling constraints
 *       required by the target MCU are respected.
 */
Platform_StatusTypeDef Platform_FLASH_Write(
  Platform_FLASH_Instance instance,
  uint32_t address,
  const void *data,
  uint32_t size
);

/**
 * @brief Erase every flash erase unit intersecting a byte range.
 * @param instance Internal flash instance.
 * @param address Start address. Must be aligned to an erase-unit boundary.
 * @param size Byte count. Greater than zero.
 * @retval Platform_OK Success.
 * @retval Platform_BUSY Another flash operation is active.
 * @retval Platform_ERROR Invalid range or HAL erase failure.
 * @warning All pages or sectors intersecting the range are erased. Use
 *          Platform_FLASH_GetEraseUnitSize() before selecting a parameter slot.
 * @note Call from task context and do not execute from the erased region.
 */
Platform_StatusTypeDef Platform_FLASH_Erase(
  Platform_FLASH_Instance instance,
  uint32_t address,
  uint32_t size
);

/**
 * @brief Get the erase-unit size covering an address.
 * @param instance Internal flash instance.
 * @param address Address to query.
 * @return Erase-unit size in bytes, or zero when the address is not in flash.
 */
uint32_t Platform_FLASH_GetEraseUnitSize(
  Platform_FLASH_Instance instance,
  uint32_t address
);

/**
 * @brief Read the latest Platform flash error snapshot.
 * @param instance Internal flash instance.
 */
uint32_t Platform_FLASH_GetLastError(Platform_FLASH_Instance instance);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_FLASH_H
