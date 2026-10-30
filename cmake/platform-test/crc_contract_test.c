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
 * @brief 查询测试替身接受的 CRC 实例。
 * @param crc CRC 实例枚举值；测试替身只接受 PLATFORM_CRC_1。
 * @retval true crc 等于 PLATFORM_CRC_1。
 * @retval false crc 为其他枚举值。
 * @post 只比较枚举值，不解析 HAL 句柄、不访问 CRC 外设，也不修改全局状态。
 * @note 这是测试替身；生产后端是否启用实例以及句柄是否有效的规则不由本函数模拟。
 */
bool platform_crc_backend_is_valid(platform_crc_instance_t crc) {
  return crc == PLATFORM_CRC_1;
}

/**
 * @brief 模拟 CRC 复位并固定报告成功，但不改变测试结果。
 * @param crc CRC 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @retval PLATFORM_STATUS_OK 固定返回，未复位任何 CRC 状态。
 * @post s_result 保持不变，不访问 CRC 数据寄存器，也不建立新的硬件计算上下文。
 * @note 测试替身不会返回生产后端声明的 ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_crc_backend_reset(
  platform_crc_instance_t crc
) {
  (void)crc;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 将测试注入的 CRC 累积结果复制到调用方存储。
 * @param crc CRC 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @param words 输入字数组；本替身忽略该参数，不读取内容，也不检查 NULL。
 * @param word_count 32 位字数量；本替身忽略该参数，不检查是否大于 0。
 * @param result 输出指针，接收 s_result 的当前值；调用方拥有存储；本替身不检查 NULL。
 * @retval PLATFORM_STATUS_OK 固定返回，并已将 s_result 写入 *result。
 * @pre result 非 NULL 且指向至少一个可写 uint32_t；违反时解引用导致未定义行为。
 * @post s_result 和 words 不变，*result 被覆盖；不执行 CRC 累积，也不访问硬件。
 * @warning 测试替身不执行参数校验，NULL result 会直接崩溃而不是返回 INVALID_ARGUMENT。
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
 * @brief 模拟直接计算 CRC，实际仅委托累积替身复制测试结果。
 * @param crc CRC 实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @param words 输入字数组；本替身不读取内容，也不检查 NULL。
 * @param word_count 32 位字数量；本替身忽略该参数，不检查是否大于 0。
 * @param result 输出指针，接收 s_result 的当前值；调用方拥有存储；本替身不检查 NULL。
 * @return platform_crc_backend_accumulate() 的返回值；当前替身固定为 PLATFORM_STATUS_OK。
 * @pre result 非 NULL 且可写；违反时解引用导致未定义行为。
 * @post 不先复位，不计算 CRC，也不访问硬件；*result 被 s_result 覆盖。
 * @note 测试替身不会返回生产后端声明的 ERROR 或 INVALID_ARGUMENT 分支。
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
 * @brief 验证 CRC 累积结果及空指针参数拒绝路径。
 * @post 测试返回时，累积输出的断言已确认等于 s_result；随后 CalculateWords
 *       传入 NULL 必须返回 PLATFORM_STATUS_INVALID_ARGUMENT，错误快照保留
 *       PLATFORM_ERROR_INVALID_ARGUMENT 位。
 * @warning 任一 assert 失败都会终止测试进程，不返回可恢复的错误状态。
 * @note 测试使用本文件内的后端桩，不访问 CRC 外设或板级硬件。
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
 * @brief 运行 CRC 契约测试并返回进程状态。
 * @return 固定返回 0；只有所有内层断言通过才会到达该返回语句。
 * @post 测试全局状态保留 CRC 契约用例最后一次操作的结果。
 * @warning 任一 assert 失败都会终止进程，因此不会返回非零退出码。
 * @note 仅供 CTest 启动本测试可执行文件时调用。
 */
int main(void) {
  test_crc_contract();
  return 0;
}
