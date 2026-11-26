/**
 * @file bsp_flash.h
 * @brief 内部 Flash 读写擦除原语（写操作要求 4 字节对齐，擦除按页/扇区对齐）。
 * @addtogroup bsp
 * @{
 */

#ifndef LAYERED_CAR_BSP_FLASH_H
#define LAYERED_CAR_BSP_FLASH_H

#include "bsp_def.h"
#include <stdint.h>

/**
 * @brief 查询覆盖 addr 的 Flash 擦除单元大小，单位 byte；非法地址返回 0。
 */
uint32_t BSP_Flash_GetEraseUnitSize(uint32_t addr);

/**
 * @brief 从内部 Flash 读取 size_bytes 字节到 out；仅任务上下文调用。
 */
bsp_status_t BSP_Flash_Read(uint32_t addr, void *out, uint32_t size_bytes);

/**
 * @brief 向已擦除内部 Flash 写入数据；addr 与 size_bytes 须按 4 字节对齐。
 */
bsp_status_t BSP_Flash_Write(uint32_t addr, const void *data, uint32_t size_bytes);

/**
 * @brief 擦除覆盖 [start_addr, start_addr + size_bytes) 的扇区；start_addr 须按扇区边界对齐。
 */
bsp_status_t BSP_Flash_Erase(uint32_t start_addr, uint32_t size_bytes);

/** @} */

#endif /* LAYERED_CAR_BSP_FLASH_H */
