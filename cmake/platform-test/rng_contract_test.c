/**
 * @file rng_contract_test.c
 * @brief RNG Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_rng_backend.h"
#include "platform_rng.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static uint32_t s_value;
static uint32_t s_backend_error;

/**
 * @brief 判断 RNG 实例是否映射到本系列的 HAL 句柄。
 * @param rng RNG 实例枚举值；测试替身仅对 PLATFORM_RNG_1 返回 true，不检查 HAL 句柄。
 * @retval true 参数等于 PLATFORM_RNG_1。
 * @retval false 参数为其他枚举值。
 * @post 只比较 rng，不访问 RNG 硬件、不读取错误快照，也不修改测试状态。
 * @note 本函数是测试替身；生产后端还会检查当前系列是否存在对应 HAL 句柄。
 */
bool platform_rng_backend_is_valid(platform_rng_instance_t rng) {
  return rng == PLATFORM_RNG_1;
}

/**
 * @brief 读取一个 32 位硬件随机数。
 * @param rng RNG 实例枚举值；测试替身忽略该参数，生产后端要求其为已启用实例。
 * @param value 输出指针；测试替身无条件写入 s_value，不得为 NULL。
 * @retval PLATFORM_STATUS_OK 测试替身已将 s_value 复制到 *value。
 * @pre value 指向至少一个 uint32_t 的可写存储；公共 RNG 层已校验实例与空指针。
 * @post *value 等于调用时的 s_value，测试替身不读取硬件，也不修改 s_value。
 * @warning 直接传 NULL 会解引用并使测试进程崩溃；生产后端的 HAL 超时、忙和生成失败路径不在本替身中模拟。
 * @note 本函数是测试替身，不访问 RNG 寄存器，不会返回 INVALID_ARGUMENT 或 ERROR；
 *       生产后端的这些状态类别仍由 Platform 公共契约保留。
 */
platform_status_t platform_rng_backend_get_random(
  platform_rng_instance_t rng,
  uint32_t *value
) {
  (void)rng;
  *value = s_value;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 返回 HAL RNG 错误快照映射后的平台错误位。
 * @param rng RNG 实例枚举值；测试替身忽略该参数，生产后端要求其为已启用实例。
 * @return s_backend_error 的当前注入值；测试可设为 NONE、UNKNOWN 或任意 RNG 错误位。
 * @post 不修改 s_backend_error，不读取 RNG 数据寄存器，也不执行 HAL 错误映射。
 * @note 本函数是测试替身；生产后端返回的是 HAL 错误快照映射后的平台错误位。
 */
uint32_t platform_rng_backend_get_error(platform_rng_instance_t rng) {
  (void)rng;
  return s_backend_error;
}

/**
 * @brief 验证 RNG 参数校验和错误映射。
 * @pre RNG 公共实现与 platform_rng_backend.h 声明的测试替身已链接。
 * @post s_value 保持为 0x12345678，s_backend_error 保持 PLATFORM_RNG_ERROR_NONE；
 *       正常路径已把该值复制到局部输出，空指针路径已置位通用非法参数错误。
 * @warning 使用 assert 校验结果；任一断言失败会调用 abort，进程不会正常返回。
 * @note 覆盖 PLATFORM_RNG_1 的测试替身成功读取、NULL 输出在公共层被拒绝及
 *       GetLastError 组合位；不验证真实 RNG 时序或 HAL 故障。
 */
static void test_rng_contract(void) {
  uint32_t value = 0U;
  s_value = 0x12345678U;
  s_backend_error = PLATFORM_RNG_ERROR_NONE;

  assert(Platform_RNG_GetRandom(PLATFORM_RNG_1, &value) == PLATFORM_STATUS_OK);
  assert(value == s_value);
  assert(Platform_RNG_GetRandom(PLATFORM_RNG_1, NULL) ==
         PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_RNG_GetLastError(PLATFORM_RNG_1) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);
}

/**
 * @brief 运行本文件定义的契约测试并返回进程状态。
 * @retval 0 test_rng_contract 的全部断言通过。
 * @pre RNG 测试替身已链接且全局 s_value、s_backend_error 可写。
 * @post 测试期间写入的 RNG 替身状态保留；没有动态内存或硬件资源。
 * @warning 断言失败不会返回错误码，而是直接终止进程。
 */
int main(void) {
  test_rng_contract();
  return 0;
}
