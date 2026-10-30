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
 * @brief 校验字节范围是否完整落在内部 Flash 地址空间内。
 * @param address 起始 Flash 字节地址；按芯片地址空间解释，本函数不检查对齐。
 * @param size 待访问字节数；必须大于零。
 * @retval true address 到 address + size - 1 的范围未越过该系列内部 Flash 末尾。
 * @retval false size 为零、address 低于 Flash 基址，或范围跨越 Flash 末尾。
 * @note 纯范围查询，不读取或修改 Flash；F1 运行期读取容量，F4 使用固定映射范围。
 * @note 可在任务或中断上下文调用，无共享状态修改。
 */
bool platform_flash_backend_range_valid(uint32_t address, uint32_t size);

/**
 * @brief 返回覆盖指定地址的 Flash 擦除单元大小。
 * @param address 待查询的 Flash 字节地址；允许位于擦除单元内部。
 * @return 覆盖该地址的擦除单元字节数；F1 为页大小，F4 为 16 KiB、64 KiB 或 128 KiB；
 *         地址不在内部 Flash 中时返回零。
 * @note 纯查询，不擦除或修改 Flash；返回值可用于计算调用方必须遵守的起始地址对齐。
 * @note 可在任务或中断上下文调用，无共享状态修改。
 */
uint32_t platform_flash_backend_erase_unit_size(uint32_t address);

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
);

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
);

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
);

/**
 * @brief 返回本编译单元保存的 Flash 后端错误快照。
 * @return PLATFORM_FLASH_ERROR_* 位掩码；PLATFORM_FLASH_ERROR_NONE 表示尚未记录错误，
 *         或最近一次后端写/擦除成功。
 * @note 只读且不清除快照；F1/F4 后端分别保存最近一次写或擦除产生的错误位。
 * @note 快照没有额外锁保护；调用方应在对应 Flash 操作结束后、同一任务上下文中读取。
 */
uint32_t platform_flash_backend_get_error(void);

#endif //LAYERED_EMBEDDED_C_PLATFORM_FLASH_BACKEND_H
