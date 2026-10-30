/**
 * @file platform_validation_test.c
 * @brief Platform 纯校验与算术辅助函数测试。
 * @ingroup tests
 */

#include "platform_validation_internal.h"

#include <stdint.h>
#include <stdio.h>

#define CHECK(condition) do {                                                \
  if (!(condition)) {                                                        \
    (void)fprintf(                                                           \
      stderr,                                                                \
      "%s:%d: check failed: %s\n",                                           \
      __FILE__,                                                              \
      __LINE__,                                                              \
      #condition                                                             \
    );                                                                       \
    return 1;                                                                \
  }                                                                          \
} while (0)

/**
 * @brief 验证 CAN 过滤器与通知参数校验。
 * @retval 0 所有 CAN 标识符和 DLC 边界检查均通过。
 * @retval 1 任一检查失败；CHECK 已向 stderr 输出文件、行号和条件。
 * @pre 可直接调用，不依赖其他测试留下的状态。
 * @post 不修改全局状态；返回前完成标准帧、扩展帧、越界标识符和超长 DLC 的校验。
 * @note 使用 CHECK 而不是 assert；失败会立即返回，不会终止整个进程。
 */
static int test_can_validation(void) {
  CHECK(platform_can_frame_values_are_valid(false, 0x000UL, 0U));
  CHECK(platform_can_frame_values_are_valid(false, 0x7FFUL, 8U));
  CHECK(!platform_can_frame_values_are_valid(false, 0x800UL, 8U));
  CHECK(platform_can_frame_values_are_valid(true, 0x1FFFFFFFUL, 8U));
  CHECK(!platform_can_frame_values_are_valid(true, 0x20000000UL, 8U));
  CHECK(!platform_can_frame_values_are_valid(false, 0x7FFUL, 9U));
  return 0;
}

/**
 * @brief 验证 SPI 参数校验。
 * @retval 0 8 位和 16 位模式下，长度、对齐和空指针边界均符合预期。
 * @retval 1 任一 CHECK 失败；失败条件及源码位置写入 stderr。
 * @pre aligned_buffer 的地址至少按 uint8_t 对齐；无需真实 SPI 外设。
 * @post 不修改被测 Platform 状态，只读取栈上缓冲区的校验结果。
 * @note 16 位模式要求非空、非零、偶数字节且起始地址偶对齐。
 */
static int test_spi_validation(void) {
  uint8_t aligned_buffer[4] = {0};
  uint8_t *unaligned_buffer = &aligned_buffer[1];

  CHECK(platform_spi_transfer_values_are_valid(aligned_buffer, 1U, false));
  CHECK(platform_spi_transfer_values_are_valid(aligned_buffer, 4U, true));
  CHECK(!platform_spi_transfer_values_are_valid(NULL, 4U, true));
  CHECK(!platform_spi_transfer_values_are_valid(aligned_buffer, 0U, false));
  CHECK(!platform_spi_transfer_values_are_valid(aligned_buffer, 3U, true));
  CHECK(!platform_spi_transfer_values_are_valid(unaligned_buffer, 2U, true));
  return 0;
}

/**
 * @brief 验证 RTC 每日闹钟计算。
 * @retval 0 普通值、日内末秒和 32 位回绕输入均得到预期结果。
 * @retval 1 任一 CHECK 失败；失败条件及源码位置写入 stderr。
 * @pre 无需 RTC 硬件；输入按无符号 32 位秒计数器解释。
 * @post 覆盖回绕语义：0xFFFFFFFF 加 86400 后按 2^32 取模得到 86399。
 * @note 不能通过饱和加法实现；该计算依赖无符号整数回绕。
 */
static int test_rtc_daily_alarm(void) {
  CHECK(platform_rtc_next_daily_alarm(0UL) == 86400UL);
  CHECK(platform_rtc_next_daily_alarm(86399UL) == 172799UL);
  CHECK(platform_rtc_next_daily_alarm(0xFFFFFFFFUL) == 86399UL);
  return 0;
}

/**
 * @brief 运行本文件定义的契约测试并返回进程状态。
 * @retval 0 三个校验用例全部通过，并向 stdout 输出成功信息。
 * @retval 1 首个失败用例返回非零；后续用例不再执行。
 * @pre 被测 Platform 纯校验辅助函数已链接到本测试程序。
 * @post 无动态内存或硬件资源残留；成功时 stdout 包含固定通过信息。
 * @note 任一 CHECK 失败已在对应辅助函数中输出源码位置和失败条件。
 */
int main(void) {
  if (test_can_validation() != 0) {
    return 1;
  }
  if (test_spi_validation() != 0) {
    return 1;
  }
  if (test_rtc_daily_alarm() != 0) {
    return 1;
  }

  (void)puts("platform validation tests passed");
  return 0;
}
