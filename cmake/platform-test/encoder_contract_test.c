/**
 * @file encoder_contract_test.c
 * @brief Encoder Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_encoder_backend.h"
#include "platform_encoder.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static uint32_t s_counter;
static uint8_t s_counter_bits;

/**
 * @brief 校验编码器实例是否已由板级配置启用。
 */
bool platform_encoder_backend_is_valid(
  platform_encoder_instance_t encoder
) {
  return encoder == PLATFORM_ENCODER_TIM2;
}

/**
 * @brief 启动编码器后端，不负责 CubeMX 结构性初始化。
 */
platform_status_t platform_encoder_backend_start(
  platform_encoder_instance_t encoder
) {
  (void)encoder;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 停止编码器后端并清理活动状态。
 */
platform_status_t platform_encoder_backend_stop(
  platform_encoder_instance_t encoder
) {
  (void)encoder;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 读取编码器硬件计数器当前值。
 */
platform_status_t platform_encoder_backend_get_counter(
  platform_encoder_instance_t encoder,
  uint32_t *counter
) {
  (void)encoder;
  *counter = s_counter;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 写入编码器硬件计数器。
 */
platform_status_t platform_encoder_backend_set_counter(
  platform_encoder_instance_t encoder,
  uint32_t counter
) {
  (void)encoder;
  s_counter = counter;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 返回编码器计数器位宽。
 */
platform_status_t platform_encoder_backend_get_counter_bits(
  platform_encoder_instance_t encoder,
  uint8_t *counter_bits
) {
  (void)encoder;
  *counter_bits = s_counter_bits;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 验证编码器参数校验和错误快照。
 */
static void test_encoder_contract(void) {
  s_counter = 0x12345678U;
  s_counter_bits = 32U;

  uint32_t counter = 0U;
  uint8_t bits = 0U;
  assert(Platform_Encoder_GetCounterBits(
           PLATFORM_ENCODER_TIM2,
           &bits
         ) == PLATFORM_STATUS_OK);
  assert(bits == 32U);
  assert(Platform_Encoder_GetCounter(
           PLATFORM_ENCODER_TIM2,
           &counter
         ) == PLATFORM_STATUS_OK);
  assert(counter == s_counter);
  assert(Platform_Encoder_SetCounter(
           PLATFORM_ENCODER_TIM2,
           7U
         ) == PLATFORM_STATUS_OK);
  assert(s_counter == 7U);
  assert(Platform_Encoder_Start(PLATFORM_ENCODER_TIM2) == PLATFORM_STATUS_OK);
  assert(Platform_Encoder_Stop(PLATFORM_ENCODER_TIM2) == PLATFORM_STATUS_OK);

  assert(Platform_Encoder_GetCounter(
           PLATFORM_ENCODER_TIM2,
           NULL
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_Encoder_GetLastError(PLATFORM_ENCODER_TIM2) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);
}

/**
 * @brief 运行本文件定义的契约测试并返回进程状态。
 */
int main(void) {
  test_encoder_contract();
  return 0;
}
