/**
 * @file crc_contract_test.c
 * @brief CRC Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_crc_backend.h"
#include "platform_crc.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static uint32_t s_result;

/**
 * @brief 校验CRC实例是否已由板级配置启用。
 */
bool platform_crc_backend_is_valid(platform_crc_instance_t crc) {
  return crc == PLATFORM_CRC_1;
}

/**
 * @brief 复位 CRC 计算状态。
 */
platform_status_t platform_crc_backend_reset(
  platform_crc_instance_t crc
) {
  (void)crc;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 累计 CRC 输入数据块。
 */
platform_status_t platform_crc_backend_accumulate(
  platform_crc_instance_t crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
) {
  (void)crc;
  (void)words;
  (void)word_count;
  *result = s_result;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 按公共 CRC 配置计算校验值。
 */
platform_status_t platform_crc_backend_calculate(
  platform_crc_instance_t crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
) {
  return platform_crc_backend_accumulate(
    crc,
    words,
    word_count,
    result
  );
}

/**
 * @brief 验证 CRC 参数校验与结果。
 */
static void test_crc_contract(void) {
  const uint32_t words[2] = {1U, 2U};
  uint32_t result = 0U;
  s_result = 0xA5A5U;

  assert(Platform_CRC_Reset(PLATFORM_CRC_1) == PLATFORM_STATUS_OK);
  assert(Platform_CRC_AccumulateWords(
           PLATFORM_CRC_1,
           words,
           2U,
           &result
         ) == PLATFORM_STATUS_OK);
  assert(result == s_result);
  assert(Platform_CRC_CalculateWords(
           PLATFORM_CRC_1,
           NULL,
           2U,
           &result
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_CRC_GetLastError(PLATFORM_CRC_1) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);
}

/**
 * @brief 运行本文件定义的契约测试并返回进程状态。
 */
int main(void) {
  test_crc_contract();
  return 0;
}
