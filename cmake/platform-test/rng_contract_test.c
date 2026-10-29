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
 * @brief 校验RNG实例是否已由板级配置启用。
 */
bool platform_rng_backend_is_valid(platform_rng_instance_t rng) {
  return rng == PLATFORM_RNG_1;
}

/**
 * @brief 读取随机数生成器输出。
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
 * @brief 返回RNG后端记录的错误位。
 */
uint32_t platform_rng_backend_get_error(platform_rng_instance_t rng) {
  (void)rng;
  return s_backend_error;
}

/**
 * @brief 验证 RNG 参数校验和错误映射。
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
 */
int main(void) {
  test_rng_contract();
  return 0;
}
