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
 * @brief 查询测试替身接受的编码器实例。
 * @param encoder 编码器实例枚举值；测试替身只接受 PLATFORM_ENCODER_TIM2。
 * @retval true encoder 等于 PLATFORM_ENCODER_TIM2。
 * @retval false encoder 为其他枚举值。
 * @post 只比较枚举值，不解析 HAL 句柄、不访问定时器，也不修改计数器。
 * @note 这是测试替身；生产后端的实例启用和句柄映射规则不由本函数模拟。
 */
bool platform_encoder_backend_is_valid(
  platform_encoder_instance_t encoder
) {
  return encoder == PLATFORM_ENCODER_TIM2;
}

/**
 * @brief 模拟启动编码器定时器并固定报告成功。
 * @param encoder 编码器实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @retval PLATFORM_STATUS_OK 固定返回，未启动编码器通道。
 * @post 不修改定时器计数、输入捕获或运行状态。
 * @note 测试替身不会返回生产后端声明的 BUSY、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_encoder_backend_start(
  platform_encoder_instance_t encoder
) {
  (void)encoder;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟停止编码器定时器并固定报告成功。
 * @param encoder 编码器实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @retval PLATFORM_STATUS_OK 固定返回，未停止编码器通道。
 * @post 不修改定时器计数、输入捕获或运行状态。
 * @note 测试替身不会返回生产后端声明的 BUSY、ERROR 或 INVALID_ARGUMENT 分支。
 */
platform_status_t platform_encoder_backend_stop(
  platform_encoder_instance_t encoder
) {
  (void)encoder;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 将测试全局计数器复制到调用方存储。
 * @param encoder 编码器实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @param counter 输出指针，接收 s_counter 的当前值；调用方拥有存储；本替身不检查 NULL。
 * @retval PLATFORM_STATUS_OK 固定返回，并已将 s_counter 写入 *counter。
 * @pre counter 非 NULL 且指向至少一个可写 uint32_t；违反时解引用导致未定义行为。
 * @post s_counter 不变，*counter 被覆盖；不读取定时器寄存器，也不计算增量。
 * @warning 测试替身不执行参数校验，NULL counter 会直接崩溃而不是返回 INVALID_ARGUMENT。
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
 * @brief 将目标值写入测试全局计数器。
 * @param encoder 编码器实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @param counter 目标 counts 值；本替身原样保存，不检查定时器位宽或范围。
 * @retval PLATFORM_STATUS_OK 固定返回，s_counter 已更新为 counter。
 * @post s_counter 等于 counter；不写定时器寄存器，也不改变编码器运行状态。
 * @note 这是测试替身；生产后端对超范围值的硬件截断行为不由本函数模拟。
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
 * @brief 将测试注入的计数器位宽复制到调用方存储。
 * @param encoder 编码器实例枚举值；本替身忽略该参数，不校验实例或句柄。
 * @param counter_bits 输出指针，接收 s_counter_bits 的当前值；调用方拥有存储；本替身不检查 NULL。
 * @retval PLATFORM_STATUS_OK 固定返回，并已将 s_counter_bits 写入 *counter_bits。
 * @pre counter_bits 非 NULL 且指向可写 uint8_t；违反时解引用导致未定义行为。
 * @post s_counter_bits 不变，*counter_bits 被覆盖；不访问定时器寄存器。
 * @warning 测试替身不执行参数校验，NULL counter_bits 会直接崩溃而不是返回 INVALID_ARGUMENT。
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
 * @brief 验证编码器位宽、计数器读写和空指针错误快照。
 * @post 成功返回时已确认位宽为 32 bit，读回值与测试全局值一致，写入值 7
 *       更新了 s_counter；Start/Stop 返回 OK；GetCounter(NULL) 返回
 *       PLATFORM_STATUS_INVALID_ARGUMENT 并记录通用参数错误位。
 * @warning 任一 assert 失败都会终止测试进程，不返回可恢复的错误状态。
 * @note 后端为本文件测试桩，不访问编码器定时器或真实硬件。
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
 * @brief 运行编码器契约测试并返回进程状态。
 * @return 固定返回 0；只有所有内层断言通过才会到达该返回语句。
 * @post 测试全局状态保留 s_counter 为 7、s_counter_bits 为 32 的用例结果。
 * @warning 任一 assert 失败都会终止进程，因此不会返回非零退出码。
 * @note 仅供 CTest 启动本测试可执行文件时调用。
 */
int main(void) {
  test_encoder_contract();
  return 0;
}
