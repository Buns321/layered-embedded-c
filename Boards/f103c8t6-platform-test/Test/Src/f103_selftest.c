/**
 * @file f103_selftest.c
 * @brief F103C8T6 非破坏性板载自检实现。
 * @ingroup tests
 */

#include "f103_selftest.h"

#include "main.h"
#include "platform_crc.h"
#include "system_log.h"

#define F103_TEST_EXPECTED_CLOCK_HZ 72000000u
#define F103_TEST_CRC_ZERO_WORD     0x00000000u
#define F103_TEST_CRC_ZERO_RESULT   0xC704DD7Bu

static void f103_selftest_report(
  const char *name,
  bool passed,
  uint32_t *passed_count,
  uint32_t *failed_count
) {
  if (passed) {
    (*passed_count)++;
    SYS_LOG_INFO("TEST", "%s PASS", name);
  } else {
    (*failed_count)++;
    SYS_LOG_WARN("TEST", "%s FAIL", name);
  }
}

static bool f103_selftest_checkclock(void) {
  bool passed = SystemCoreClock == F103_TEST_EXPECTED_CLOCK_HZ;

  if (!passed) {
    SYS_LOG_WARN(
      "TEST",
      "clock expected=%lu actual=%lu",
      (unsigned long)F103_TEST_EXPECTED_CLOCK_HZ,
      (unsigned long)SystemCoreClock
    );
  }
  return passed;
}

static bool f103_selftest_checklog(void) {
  bool is_ready = Sys_Log_IsReady();
  uint32_t drop_bytes = Sys_Log_GetDropBytes();
  uint32_t contention_count = Sys_Log_GetContentionCount();
  bool passed =
    is_ready &&
    drop_bytes == 0u &&
    contention_count == 0u;

  if (!passed) {
    SYS_LOG_WARN(
      "TEST",
      "log ready=%u drop=%lu contention=%lu",
      is_ready ? 1u : 0u,
      (unsigned long)drop_bytes,
      (unsigned long)contention_count
    );
  }
  return passed;
}

static bool f103_selftest_checkcrc(void) {
  const uint32_t words[] = {F103_TEST_CRC_ZERO_WORD};
  uint32_t result = 0u;
  platform_status_t status = Platform_CRC_CalculateWords(
    PLATFORM_CRC_1,
    words,
    1u,
    &result
  );
  bool passed =
    status == PLATFORM_STATUS_OK &&
    result == F103_TEST_CRC_ZERO_RESULT;

  if (!passed) {
    SYS_LOG_WARN(
      "TEST",
      "crc status=%lu expected=0x%08lX actual=0x%08lX",
      (unsigned long)status,
      (unsigned long)F103_TEST_CRC_ZERO_RESULT,
      (unsigned long)result
    );
  }
  return passed;
}

bool F103_SelfTest_RunAll(
  bool scheduler_tick_ok,
  uint32_t scheduler_tick_elapsed_ms
) {
  uint32_t passed_count = 0u;
  uint32_t failed_count = 0u;

  SYS_LOG_INFO("TEST", "BEGIN board=f103c8t6");

  f103_selftest_report(
    "clock",
    f103_selftest_checkclock(),
    &passed_count,
    &failed_count
  );
  SYS_LOG_INFO(
    "TEST",
    "tick elapsed=%lu ms",
    (unsigned long)scheduler_tick_elapsed_ms
  );
  f103_selftest_report(
    "tick",
    scheduler_tick_ok,
    &passed_count,
    &failed_count
  );
  f103_selftest_report(
    "log",
    f103_selftest_checklog(),
    &passed_count,
    &failed_count
  );
  f103_selftest_report(
    "crc",
    f103_selftest_checkcrc(),
    &passed_count,
    &failed_count
  );

  SYS_LOG_INFO(
    "TEST",
    "SUMMARY pass=%lu fail=%lu",
    (unsigned long)passed_count,
    (unsigned long)failed_count
  );

  return failed_count == 0u;
}
