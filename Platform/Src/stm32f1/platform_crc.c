/**
 * @file platform_crc.c
 * @brief STM32F1 公共 CRC 实现的 HAL 后端。
 * @ingroup platform
 */

#include "crc.h"
#include "platform_crc_backend.h"
#include "stm32f1xx_hal.h"

/**
 * @brief 把平台 CRC 实例编号转换为 HAL 句柄。
 * @param crc CRC 实例枚举值；当前仅 PLATFORM_CRC_1 有定义。
 * @return 对应的 CRC HAL 句柄；枚举值非法时返回 NULL。
 * @post 只执行实例映射，不访问 CRC 寄存器，也不修改任何状态。
 * @note 内部辅助函数，仅在任务或中断上下文执行只读映射，不分配动态内存。
 */
static CRC_HandleTypeDef *platform_crc_to_hal(platform_crc_instance_t crc) {
  return crc == PLATFORM_CRC_1 ? &hcrc : NULL;
}

/**
 * @brief 查询 CRC 实例是否映射到可用的 HAL 句柄。
 * @param crc CRC 实例枚举值；当前仅 PLATFORM_CRC_1 有定义。
 * @retval true 实例有效且映射到 CRC HAL 句柄。
 * @retval false 枚举值非法或没有 HAL 句柄。
 * @post 只执行映射查询，不访问 CRC 数据寄存器、不改变计算状态。
 * @note 可在任务或中断上下文调用；本查询不阻塞且不分配动态内存。
 */
bool platform_crc_backend_is_valid(platform_crc_instance_t crc) {
  return platform_crc_to_hal(crc) != NULL;
}

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
) {
  CRC_HandleTypeDef *crc_h = platform_crc_to_hal(crc);
  if (crc_h == NULL || crc_h->State != HAL_CRC_STATE_READY) {
    return crc_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT : PLATFORM_STATUS_ERROR;
  }
  __HAL_CRC_DR_RESET(crc_h);
  return PLATFORM_STATUS_OK;
}

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
) {
  CRC_HandleTypeDef *crc_h = platform_crc_to_hal(crc);
  if (crc_h == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (crc_h->State != HAL_CRC_STATE_READY) {
    return PLATFORM_STATUS_ERROR;
  }
  *result = HAL_CRC_Accumulate(crc_h, (uint32_t *)words, word_count);
  return PLATFORM_STATUS_OK;
}

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
) {
  CRC_HandleTypeDef *crc_h = platform_crc_to_hal(crc);
  if (crc_h == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (crc_h->State != HAL_CRC_STATE_READY) {
    return PLATFORM_STATUS_ERROR;
  }
  *result = HAL_CRC_Calculate(crc_h, (uint32_t *)words, word_count);
  return PLATFORM_STATUS_OK;
}
