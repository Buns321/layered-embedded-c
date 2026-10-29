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
 * @brief 构造 CRC 非法参数状态。
 */
static platform_status_t platform_crc_invalid(void) {
  platform_error_record(&crc_error, PLATFORM_ERROR_INVALID_ARGUMENT);
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

/**
 * @brief 把 HAL CRC 结果转换为公共状态并记录错误。
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
 * @param crc CRC 实例。
 * @pre MX_CRC_Init() 已成功完成。
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
 * @param crc CRC 实例。
 * @param words 输入字数组。
 * @param word_count 32 位字数量，必须大于 0。
 * @param result 累积计算后的 CRC 数据寄存器值输出。
 * @note STM32F4 的 CRC 单元按 32 位字处理数据。
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
 * @param crc CRC 实例。
 * @param words 输入字数组。
 * @param word_count 32 位字数量，必须大于 0。
 * @param result 计算得到的 CRC 数据寄存器值输出。
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

/** @brief 读取最近一次 CRC 操作错误快照。 */
uint32_t Platform_CRC_GetLastError(platform_crc_instance_t crc) {
  return platform_crc_backend_is_valid(crc)
           ? crc_error
           : PLATFORM_ERROR_INVALID_ARGUMENT;
}
