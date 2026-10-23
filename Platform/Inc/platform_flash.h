//
// 由 Buns_ 创建于 2026/10/23。
//

/**
 * @file platform_flash.h
 * @brief 不依赖 HAL 类型的内部 Flash 基础操作。
 * @addtogroup platform
 * @{
 * @note 本模块只负责底层读取、写入和擦除操作。参数布局、CRC 校验、磨损策略
 *       以及默认值回退属于 System 参数存储层。
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_FLASH_H
#define LAYERED_EMBEDDED_C_PLATFORM_FLASH_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

#if !PLATFORM_ENABLE_FLASH
#error "Platform flash must be enabled before including platform_flash.h"
#endif

/** @brief 可用的内部 Flash 实例。 */
typedef enum platform_flash_instance_e {
  PLATFORM_FLASH_1 = 0U,
} platform_flash_instance_t;

/** @brief 内部 Flash 错误标志。 */
typedef enum platform_flash_error_e {
  PLATFORM_FLASH_ERROR_NONE          = 0u,
  PLATFORM_FLASH_ERROR_PROGRAM       = (1u << 0),
  PLATFORM_FLASH_ERROR_ERASE         = (1u << 1),
  PLATFORM_FLASH_ERROR_WRITE_PROTECT = (1u << 2),
  PLATFORM_FLASH_ERROR_VERIFY        = (1u << 3),
  PLATFORM_FLASH_ERROR_ALIGNMENT     = (1u << 4),
  PLATFORM_FLASH_ERROR_RANGE         = (1u << 5),
  PLATFORM_FLASH_ERROR_INTERNAL      = (1u << 6),
  PLATFORM_FLASH_ERROR_UNKNOWN       = (1u << 7),
} platform_flash_error_t;

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
);

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
);

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
);

/**
 * @brief 获取覆盖指定地址的擦除单元大小。
 * @param instance 内部 Flash 实例。
 * @param address 待查询地址。
 * @return 擦除单元大小（字节）；地址不在 Flash 中时返回零。
 */
uint32_t Platform_FLASH_GetEraseUnitSize(
  platform_flash_instance_t instance,
  uint32_t address
);

/**
 * @brief 读取最近一次 Platform Flash 错误快照。
 * @param instance 内部 Flash 实例。
 */
uint32_t Platform_FLASH_GetLastError(platform_flash_instance_t instance);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_FLASH_H
