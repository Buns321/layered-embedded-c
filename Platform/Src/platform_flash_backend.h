/**
 * @file platform_flash_backend.h
 * @brief STM32 系列公共 Flash 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_FLASH_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_FLASH_BACKEND_H

#include "platform_def.h"
#include "platform_flash.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 校验 Flash 地址范围与对齐是否合法。
 */
bool platform_flash_backend_range_valid(uint32_t address, uint32_t size);

/**
 * @brief 返回地址所在 Flash 擦除单元大小。
 */
uint32_t platform_flash_backend_erase_unit_size(uint32_t address);

/**
 * @brief 读取Flash数据。
 */
platform_status_t platform_flash_backend_read(
  uint32_t address,
  void *data,
  uint32_t size
);

/**
 * @brief 写入Flash数据。
 */
platform_status_t platform_flash_backend_write(
  uint32_t address,
  const void *data,
  uint32_t size
);

/**
 * @brief 擦除 Flash 指定地址范围。
 */
platform_status_t platform_flash_backend_erase(
  uint32_t address,
  uint32_t size
);

/**
 * @brief 返回Flash后端记录的错误位。
 */
uint32_t platform_flash_backend_get_error(void);

#endif //LAYERED_EMBEDDED_C_PLATFORM_FLASH_BACKEND_H
