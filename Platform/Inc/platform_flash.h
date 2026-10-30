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
 * @param instance 内部 Flash 实例；当前只支持 PLATFORM_FLASH_1。
 * @param address MCU Flash 地址空间中的起始地址，单位字节。
 * @param data 目标缓冲区；函数只写 size 字节，所有权仍归调用方。
 * @param size 读取字节数，必须大于零且完整落在 Flash 范围内。
 * @retval PLATFORM_STATUS_OK 读取成功，data 指向的 size 字节已从 Flash 复制。
 * @retval PLATFORM_STATUS_BUSY 另一个 Flash 操作正在进行。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法、data 为空或地址范围非法。
 * @retval PLATFORM_STATUS_ERROR 后端读取失败。
 * @note 从任务上下文调用。应用中的所有 Flash 操作都必须使用这个 Platform
 *       实例，以确保忙状态保护具有唯一权威性。
 * @note 忙状态受 System 临界区保护，但数据缓冲区所有权和并发修改由调用方负责。
 */
platform_status_t Platform_FLASH_Read(
  platform_flash_instance_t instance,
  uint32_t address,
  void *data,
  uint32_t size
);

/**
 * @brief 向已擦除的内部 Flash 写入 32 位字。
 * @param instance 内部 Flash 实例；当前只支持 PLATFORM_FLASH_1。
 * @param address 目标地址，单位字节，必须按 4 字节对齐并位于 Flash 范围。
 * @param data 源缓冲区，按 32 位字读取；调用期间保持只读。
 * @param size 写入字节数，必须是四的非零整数倍。
 * @retval PLATFORM_STATUS_OK 写入成功，size 字节已按 32 位字写入目标区域。
 * @retval PLATFORM_STATUS_BUSY 另一个 Flash 操作正在进行。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、缓冲区、地址范围或对齐非法。
 * @retval PLATFORM_STATUS_ERROR HAL 写入失败。
 * @note 目标区域必须已擦除，擦除策略由调用方负责。
 * @note 从任务上下文调用，并遵守目标 MCU 对中断或 RTOS 调度的要求。
 * @warning 写入期间不得从目标 Flash 区域执行代码；失败后错误快照保留具体 Flash 错误位。
 */
platform_status_t Platform_FLASH_Write(
  platform_flash_instance_t instance,
  uint32_t address,
  const void *data,
  uint32_t size
);

/**
 * @brief 擦除与给定字节范围相交的所有 Flash 擦除单元。
 * @param instance 内部 Flash 实例；当前只支持 PLATFORM_FLASH_1。
 * @param address 起始地址，单位字节，必须按擦除单元边界对齐。
 * @param size 擦除范围字节数，必须大于零且完整落在 Flash 范围内。
 * @retval PLATFORM_STATUS_OK 擦除成功，所有与请求范围相交的擦除单元均已完成。
 * @retval PLATFORM_STATUS_BUSY 另一个 Flash 操作正在进行。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址范围或擦除单元对齐非法。
 * @retval PLATFORM_STATUS_ERROR HAL 擦除失败。
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
 * @param instance 内部 Flash 实例；当前只支持 PLATFORM_FLASH_1。
 * @param address 待查询地址，单位字节。
 * @retval 0 实例非法或地址不在 Flash 范围内。
 * @retval 大于0 覆盖该地址的页或扇区大小，单位为字节。
 * @note 纯查询，不修改 Flash、不改变忙状态，也不分配内存。
 */
uint32_t Platform_FLASH_GetEraseUnitSize(
  platform_flash_instance_t instance,
  uint32_t address
);

/**
 * @brief 读取最近一次 Platform Flash 错误快照。
 * @param instance 内部 Flash 实例；当前只支持 PLATFORM_FLASH_1，其他值返回
 *        PLATFORM_ERROR_INVALID_ARGUMENT。
 * @return PLATFORM_FLASH_ERROR_* 与 PLATFORM_ERROR_* 的位掩码组合；非法实例返回
 *         PLATFORM_ERROR_INVALID_ARGUMENT。
 * @note 只读，不清除快照；后续公共 Flash 操作开始时会清除快照。
 * @note 不访问 Flash 硬件，不分配内存；并发读写快照须由调用方串行化。
 */
uint32_t Platform_FLASH_GetLastError(platform_flash_instance_t instance);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_FLASH_H
