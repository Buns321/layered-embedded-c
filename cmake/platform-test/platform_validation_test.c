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

static int test_can_validation(void) {
  CHECK(platform_can_frame_values_are_valid(false, 0x000UL, 0U));
  CHECK(platform_can_frame_values_are_valid(false, 0x7FFUL, 8U));
  CHECK(!platform_can_frame_values_are_valid(false, 0x800UL, 8U));
  CHECK(platform_can_frame_values_are_valid(true, 0x1FFFFFFFUL, 8U));
  CHECK(!platform_can_frame_values_are_valid(true, 0x20000000UL, 8U));
  CHECK(!platform_can_frame_values_are_valid(false, 0x7FFUL, 9U));
  return 0;
}

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

static int test_rtc_daily_alarm(void) {
  CHECK(platform_rtc_next_daily_alarm(0UL) == 86400UL);
  CHECK(platform_rtc_next_daily_alarm(86399UL) == 172799UL);
  CHECK(platform_rtc_next_daily_alarm(0xFFFFFFFFUL) == 86399UL);
  return 0;
}

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
