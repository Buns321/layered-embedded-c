//
// 由 Buns_ 创建于 2026/10/22。
//

/**
 * @file platform_crc.h
 * @brief STM32 硬件 CRC 单元的不依赖 HAL 类型接口。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_CRC_H
#define LAYERED_EMBEDDED_C_PLATFORM_CRC_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

#if !PLATFORM_ENABLE_CRC
#error "Platform CRC is not enabled"
#endif

/** @brief 可用的硬件 CRC 身份。 */
typedef enum platform_crc_instance_e {
  PLATFORM_CRC_1 = 0U,
} platform_crc_instance_t;

/**
 * @brief 复位 CRC 数据寄存器。
 * @param crc 已启用的硬件 CRC 实例。
 * @retval PLATFORM_STATUS_OK CRC 数据寄存器已恢复为外设初始值。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例未启用或非法。
 * @retval PLATFORM_STATUS_ERROR HAL 复位失败。
 * @pre MX_CRC_Init() 已成功完成。
 * @note 同一 CRC 实例的累积、计算和复位必须由调用方串行化；不分配内存。
 */
platform_status_t Platform_CRC_Reset(platform_crc_instance_t crc);

/**
 * @brief 在不复位 CRC 单元的情况下累积计算 32 位字。
 * @param crc 已启用的硬件 CRC 实例。
 * @param words 输入字数组，按当前 CRC 配置逐字累积；调用期间只读。
 * @param word_count 输入 32 位字数量，必须大于 0，不是字节数。
 * @param result 累积后的 CRC 数据寄存器值输出地址，单位由 CRC 配置决定。
 * @retval PLATFORM_STATUS_OK words 已处理且 result 已写入。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、words、word_count 或 result 非法。
 * @retval PLATFORM_STATUS_ERROR HAL 累积计算失败。
 * @pre MX_CRC_Init() 已成功完成，words 在调用期间保持有效且可读。
 * @note words 和 result 不要求重叠；同一 CRC 实例的操作须串行化。
 */
platform_status_t Platform_CRC_AccumulateWords(
  platform_crc_instance_t crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
);

/**
 * @brief 复位后计算 32 位字数组的 CRC。
 * @param crc 已启用的硬件 CRC 实例。
 * @param words 输入字数组；调用期间只读。
 * @param word_count 输入 32 位字数量，必须大于 0，不是字节数。
 * @param result 计算结果输出地址，单位由 CRC 配置决定。
 * @retval PLATFORM_STATUS_OK CRC 已复位并完成计算，result 已写入。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、words、word_count 或 result 非法。
 * @retval PLATFORM_STATUS_ERROR HAL 复位或计算失败。
 * @pre MX_CRC_Init() 已成功完成，words 在调用期间保持有效且可读。
 * @note 本函数先复位再计算，结果与之前累积内容无关；同一实例须串行化。
 */
platform_status_t Platform_CRC_CalculateWords(
  platform_crc_instance_t crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
);

/**
 * @brief 读取最近一次 CRC 操作错误快照。
 * @param crc CRC 实例；当前仅支持 PLATFORM_CRC_1，非法值返回 PLATFORM_ERROR_INVALID_ARGUMENT。
 * @return PLATFORM_ERROR_* 位掩码；非法实例返回 PLATFORM_ERROR_INVALID_ARGUMENT。
 * @note 只读，不清除软件快照；后续公共 CRC 操作开始时会清除快照。
 * @note 本函数不读取 CRC 数据寄存器，也不修改硬件状态。
 */
uint32_t Platform_CRC_GetLastError(platform_crc_instance_t crc);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_CRC_H
