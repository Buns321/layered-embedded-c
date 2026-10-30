/**
 * @file platform_crc_backend.h
 * @brief STM32 系列公共 CRC 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_CRC_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_CRC_BACKEND_H

#include "platform_crc.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 查询 CRC 实例是否映射到可用的 HAL 句柄。
 * @param crc CRC 实例枚举值；当前仅 PLATFORM_CRC_1 有定义。
 * @retval true 实例有效且映射到 CRC HAL 句柄。
 * @retval false 枚举值非法或没有 HAL 句柄。
 * @post 只执行映射查询，不访问 CRC 数据寄存器、不改变计算状态。
 * @note 可在任务或中断上下文调用；本查询不阻塞且不分配动态内存。
 */
bool platform_crc_backend_is_valid(platform_crc_instance_t crc);

/**
 * @brief 复位 CRC 数据寄存器并开始新的计算上下文。
 * @param crc CRC 实例枚举值；当前仅 PLATFORM_CRC_1 有定义。
 * @retval PLATFORM_STATUS_OK CRC 数据寄存器已复位。
 * @retval PLATFORM_STATUS_ERROR CRC HAL 状态不是 READY，未执行复位。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或没有 HAL 句柄。
 * @pre MX_CRC_Init() 已完成，且没有其他调用正在并发使用同一 CRC 单元。
 * @post 成功后后续累积从硬件定义的初值开始；本函数不保存历史结果。
 * @note CRC 单元为共享硬件资源，调用方必须串行化访问；本函数不分配动态内存。
 */
platform_status_t platform_crc_backend_reset(
  platform_crc_instance_t crc
);

/**
 * @brief 在现有 CRC 状态上累积一个 32 位字数组。
 * @param crc CRC 实例枚举值；当前仅 PLATFORM_CRC_1 有定义。
 * @param words 输入字数组，非 NULL；调用方拥有，调用期间保持只读可访问。
 * @param word_count 32 位字数量，必须大于 0。
 * @param result 输出指针，接收累积后的 CRC 数据寄存器值；调用方拥有存储，非 NULL。
 * @retval PLATFORM_STATUS_OK 已累积全部字并写入 *result。
 * @retval PLATFORM_STATUS_ERROR CRC HAL 状态不是 READY。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或没有 HAL 句柄。
 * @pre words 非 NULL 且指向至少 word_count 个 uint32_t；word_count 大于 0；result 非 NULL、
 *      可写且生命周期覆盖本次调用。
 * @post 成功后 CRC 数据寄存器包含累积结果，*result 保存同一快照值；本函数不复位 CRC。
 * @note 不复制输入数组、不分配动态内存；同一 CRC 单元必须由调用方串行化访问。
 */
platform_status_t platform_crc_backend_accumulate(
  platform_crc_instance_t crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
);

/**
 * @brief 复位后计算一个 32 位字数组的 CRC。
 * @param crc CRC 实例枚举值；当前仅 PLATFORM_CRC_1 有定义。
 * @param words 输入字数组，非 NULL；调用方拥有，调用期间保持只读可访问。
 * @param word_count 32 位字数量，必须大于 0。
 * @param result 输出指针，接收计算后的 CRC 数据寄存器值；调用方拥有存储，非 NULL。
 * @retval PLATFORM_STATUS_OK 已复位 CRC、处理全部字并写入 *result。
 * @retval PLATFORM_STATUS_ERROR CRC HAL 状态不是 READY，计算未执行。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或没有 HAL 句柄。
 * @pre words 非 NULL 且指向至少 word_count 个 uint32_t；word_count 大于 0；result 非 NULL、
 *      可写且生命周期覆盖本次调用。
 * @post 成功后 CRC 数据寄存器与 *result 均为本次结果；历史累积状态被覆盖。
 * @note STM32F1/F4 的 CRC 单元按 32 位字处理；不复制输入数组，也不分配动态内存。
 */
platform_status_t platform_crc_backend_calculate(
  platform_crc_instance_t crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_CRC_BACKEND_H
