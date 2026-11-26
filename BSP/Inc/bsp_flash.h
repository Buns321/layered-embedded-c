/**
 * @file bsp_flash.h
 * @brief 内部 Flash 读写擦除原语（写操作要求 4 字节对齐，擦除按扇区边界对齐）。
 * @details 提供与具体芯片扇区布局解耦的片上 Flash 访问接口，当前后端实装 STM32F407VET6
 *          （Sector 0..3 为 16 KiB、Sector 4 为 64 KiB、Sector 5..7 为 128 KiB）。
 *          内部通过临界区互斥标志防止并发擦写，所有操作仅限任务上下文执行。
 *
 * @addtogroup BSP
 * @{
 */

#ifndef LAYERED_CAR_BSP_FLASH_H
#define LAYERED_CAR_BSP_FLASH_H

#include "bsp_def.h"
#include <stdint.h>

/**
 * @brief 查询覆盖指定地址的 Flash 擦除单元（扇区）大小。
 * @param[in] addr 片上 Flash 目标字节地址（如 0x08060000）。
 * @return 对应扇区的字节大小（单位：byte，STM32F4 为 16384 / 65536 / 131072）；
 *         地址越界或非法时返回 0。
 * @note 纯只读查表操作，不访问 Flash 控制器寄存器。
 */
uint32_t BSP_Flash_GetEraseUnitSize(uint32_t addr);

/**
 * @brief 从内部 Flash 读取指定字节数到目标缓冲区。
 * @param[in]  addr       片上 Flash 起始字节地址。
 * @param[out] out        接收数据的目标缓冲区指针（不允许为 NULL）。
 * @param[in]  size_bytes 读取字节数（单位：byte，须 > 0 且不越出 Flash 边界）。
 * @return 操作状态码。
 * @retval BSP_OK              读取成功。
 * @retval BSP_ERR_INVALID_ARG `out` 为 NULL、`size_bytes` 为 0 或地址区间越界。
 * @retval BSP_ERR_BUSY        当前已有其他 Flash 操作正在进行。
 * @note 仅限任务上下文调用；允许任意字节对齐读取。
 */
bsp_status_t BSP_Flash_Read(uint32_t addr, void *out, uint32_t size_bytes);

/**
 * @brief 向已擦除内部 Flash 区域按字（4 字节）编程写入数据。
 * @param[in] addr       片上 Flash 起始字节地址（须 4 字节对齐）。
 * @param[in] data       待写入数据缓冲区指针（不允许为 NULL）。
 * @param[in] size_bytes 写入字节数（单位：byte，须 > 0、为 4 的倍数且不越出 Flash 边界）。
 * @return 操作状态码。
 * @retval BSP_OK              写入成功。
 * @retval BSP_ERR_INVALID_ARG 指针为空、区间越界，或 `addr`/`size_bytes` 未按 4 字节对齐。
 * @retval BSP_ERR_BUSY        当前已有其他 Flash 操作正在进行。
 * @retval BSP_ERR_HAL         底层 HAL_FLASH_Program() 失败（如目标未擦除）。
 * @pre 目标区域须事先经 BSP_Flash_Erase() 擦除为全 0xFF。
 * @warning 仅限任务上下文调用；STM32F4 单 Bank 编程期间 CPU 取指会暂停，勿在强实时路径调用。
 */
bsp_status_t BSP_Flash_Write(uint32_t addr, const void *data, uint32_t size_bytes);

/**
 * @brief 擦除覆盖 `[start_addr, start_addr + size_bytes)` 区间的所有 Flash 扇区。
 * @param[in] start_addr 起始字节地址（必须严格对齐到所在扇区起始边界）。
 * @param[in] size_bytes 待擦除字节长度（单位：byte，须 > 0 且不越出 Flash 边界）。
 * @return 操作状态码。
 * @retval BSP_OK              扇区擦除成功。
 * @retval BSP_ERR_INVALID_ARG 区间越界或 `start_addr` 未按扇区边界对齐。
 * @retval BSP_ERR_BUSY        当前已有其他 Flash 操作正在进行。
 * @retval BSP_ERR_HAL         底层 HAL_FLASHEx_Erase() 返回错误或 `sector_error != 0xFFFFFFFF`。
 * @warning 擦除整扇区会清空该扇区内所有数据（即使 `size_bytes` 小于扇区大小）；
 *          仅限任务上下文调用，擦除期间 CPU 取指会阻塞数十至数百毫秒。
 */
bsp_status_t BSP_Flash_Erase(uint32_t start_addr, uint32_t size_bytes);

#endif /* LAYERED_CAR_BSP_FLASH_H */

/** @} */
