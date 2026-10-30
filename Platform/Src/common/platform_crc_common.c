/**
 * @file platform_crc_common.c
 * @brief 与芯片系列无关的 CRC 参数校验与错误快照。
 * @ingroup platform
 */

#include "platform_crc_backend.h"
#include "platform_error_internal.h"
#include <stddef.h>

static volatile uint32_t crc_error;

/**
 * @brief 记录 CRC 参数错误并构造统一失败状态。
 * @return 始终返回 PLATFORM_STATUS_INVALID_ARGUMENT。
 * @post 软件错误快照置入 PLATFORM_ERROR_INVALID_ARGUMENT。
 * @note 仅供本文件失败路径使用；不访问 CRC 硬件，不分配内存。
 */
static platform_status_t platform_crc_invalid(void) {
  platform_error_record(&crc_error, PLATFORM_ERROR_INVALID_ARGUMENT);
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

/**
 * @brief 把 CRC 后端结果转换为公共状态并保留失败快照。
 * @param status 后端返回的平台状态。
 * @return 原样返回 status，便于调用方直接返回。
 * @post status 非 OK 时把对应通用错误位合并到软件快照；OK 不修改快照。
 * @note 仅供本文件公共操作收尾使用；不访问硬件，不分配内存。
 */
static platform_status_t platform_crc_finish(
  platform_status_t status
) {
  if (status != PLATFORM_STATUS_OK) {
    platform_error_record(&crc_error, platform_error_from_status(status));
  }
  return status;
}

/**
 * @brief 复位 CRC 数据寄存器。
 * @param crc 已启用的硬件 CRC 实例。
 * @retval PLATFORM_STATUS_OK CRC 数据寄存器已恢复为外设初始值。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例未启用或非法。
 * @retval PLATFORM_STATUS_ERROR HAL 复位失败。
 * @pre MX_CRC_Init() 已成功完成。
 * @note 同一 CRC 实例的累积、计算和复位必须由调用方串行化；不分配内存。
 */
platform_status_t Platform_CRC_Reset(platform_crc_instance_t crc) {
  platform_error_reset(&crc_error);
  if (!platform_crc_backend_is_valid(crc)) {
    return platform_crc_invalid();
  }
  return platform_crc_finish(platform_crc_backend_reset(crc));
}

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
) {
  platform_error_reset(&crc_error);
  if (
    !platform_crc_backend_is_valid(crc) ||
    words == NULL ||
    word_count == 0U ||
    result == NULL
  ) {
    return platform_crc_invalid();
  }
  return platform_crc_finish(
    platform_crc_backend_accumulate(crc, words, word_count, result)
  );
}

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
) {
  platform_error_reset(&crc_error);
  if (
    !platform_crc_backend_is_valid(crc) ||
    words == NULL ||
    word_count == 0U ||
    result == NULL
  ) {
    return platform_crc_invalid();
  }
  return platform_crc_finish(
    platform_crc_backend_calculate(crc, words, word_count, result)
  );
}

/**
 * @brief 读取最近一次 CRC 操作错误快照。
 * @param crc CRC 实例；当前仅支持 PLATFORM_CRC_1，非法值返回 PLATFORM_ERROR_INVALID_ARGUMENT。
 * @return PLATFORM_ERROR_* 位掩码；非法实例返回 PLATFORM_ERROR_INVALID_ARGUMENT。
 * @note 只读，不清除软件快照；后续公共 CRC 操作开始时会清除快照。
 * @note 本函数不读取 CRC 数据寄存器，也不修改硬件状态。
 */
uint32_t Platform_CRC_GetLastError(platform_crc_instance_t crc) {
  return platform_crc_backend_is_valid(crc)
           ? crc_error
           : PLATFORM_ERROR_INVALID_ARGUMENT;
}
