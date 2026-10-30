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

/**
 * @brief 按测试结果累加计数并输出一条 PASS/FAIL 日志。
 * @param name 测试项名称，非 NULL 的 NUL 结尾字符串；不复制，调用期间保持有效。
 * @param passed true 计入通过并输出 PASS；false 计入失败并输出 FAIL。
 * @param passed_count 通过计数；必须指向调用方持有的有效计数器，通过分支递增。
 * @param failed_count 失败计数；必须指向调用方持有的有效计数器，失败分支递增。
 * @pre name、passed_count、failed_count 三个指针均非 NULL，计数器可安全递增。
 * @post 恰好递增一个计数器，并尝试输出对应等级的日志；不影响其他测试计数。
 * @note 本编译单元私有辅助函数；日志受过滤和并发丢弃影响，不影响计数。
 */
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

/**
 * @brief 检查 SystemCoreClock 是否等于 72 MHz。
 * @retval true SystemCoreClock 等于 72000000。
 * @retval false 不相等；输出 expected/actual 两项的 WARN 日志。
 * @pre SystemCoreClock 已由 HAL 和时钟初始化更新为当前内核频率。
 * @post 仅可能写日志，不修改时钟配置或全局状态。
 * @note 本编译单元私有辅助函数；只比较运行时变量，不验证 HSE、PLL 源或寄存器配置。
 */
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

/**
 * @brief 检查 RTT 日志后端就绪且启动以来无丢弃和竞争。
 * @retval true 后端已配置，Sys_Log_GetDropBytes() 和
 *         Sys_Log_GetContentionCount() 均返回 0。
 * @retval false 任一条件不满足；输出 ready/drop/contention 的 WARN 日志。
 * @pre Sys_Log 已完成初始化，统计计数器可读。
 * @post 仅可能写日志；不清零 Sys_Log 的丢弃字节数和竞争条数。
 * @note 本编译单元私有辅助函数；统计从日志模块初始化起累计，包含自检之前
 *       其他任务的写入，且本检查之后的日志仍可能改变计数。
 */
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

/**
 * @brief 用硬件 CRC 计算单个零字并比对固定结果。
 * @retval true Platform_CRC_CalculateWords 返回 OK 且结果为 0xC704DD7B。
 * @retval false 状态非 OK 或结果不匹配；输出 status/expected/actual WARN 日志。
 * @pre MX_CRC_Init() 已完成，PLATFORM_CRC_1 时钟可用，且无其他 CRC 使用者并发访问。
 * @post CRC 数据寄存器保留本次计算结果，不恢复调用前的值。
 * @note 本编译单元私有辅助函数；输入为 1 个 0x00000000 字，期望值对应 STM32
 *       硬件 CRC-32 复位后计算该字的结果。
 * @warning 硬件 CRC 单元为共享外设，本检查会复位其数据寄存器，不可与通信或
 *          存储校验流程并发执行。
 */
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

/**
 * @brief 运行一次非破坏性板载自检，检查时钟、tick、日志和 CRC。
 * @param scheduler_tick_ok 调用方对 FreeRTOS tick 健康状态的判断；true 计入
 *        tick 项通过，false 计入失败。
 * @param scheduler_tick_elapsed_ms 调用方测得的 tick 延时，单位 ms；仅写入日志，
 *        不参与通过/失败判定。
 * @retval true clock、tick、log、crc 四项全部通过（failed_count 为 0）。
 * @retval false 至少一项失败；失败项由 TEST 标签 WARN 日志给出。
 * @pre 已在 CubeMX 初始化完成、RTOS 调度器启动后的任务上下文调用；
 *      RTT 日志后端和硬件 CRC 外设可用。
 * @post 输出 BEGIN、各项结果和 SUMMARY；计数只保存在本函数局部变量中。
 * @note 不操作继电器、电机、舵机、蜂鸣器和灯；只读取 SystemCoreClock、日志统计，
 *       并对硬件 CRC 单元执行一次计算。
 * @note CRC 项会复位并占用 PLATFORM_CRC_1，结束后其数据寄存器保留本次 CRC 值。
 * @warning 日志项读取全局丢弃/竞争计数，启动期任何日志异常都会使该项失败；
 *          自检自身的日志也可能被过滤或丢弃；实现不使用 assert，失败通过
 *          返回值和日志呈现。
 */
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
