/**
 * @file rtc_contract_test.c
 * @brief RTC Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_rtc_backend.h"
#include "platform_rtc.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static bool s_has_wakeup;
static platform_status_t s_set_time_status;
static platform_status_t s_set_date_status;
static platform_status_t s_set_alarm_status;
static platform_rtc_date_time_t s_datetime;
static uint32_t s_alarm_count;
static uint32_t s_wakeup_count;

/**
 * @brief 返回测试注入的 RTC 唤醒定时器能力。
 * @retval true s_has_wakeup 当前为 true。
 * @retval false s_has_wakeup 当前为 false。
 * @post 只读取 s_has_wakeup，不检查 RTC 初始化状态，也不访问硬件。
 * @note 测试通过直接写 s_has_wakeup 切换能力；生产后端的系列能力不由本函数模拟。
 */
bool platform_rtc_backend_has_wakeup_timer(void) {
  return s_has_wakeup;
}

/**
 * @brief 将时分秒复制到测试日期时间快照并返回注入状态。
 * @param date_time 日期时间输入；读取 hour、minute、second，其他字段忽略；本替身不检查 NULL。
 * @return s_set_time_status 的当前值；测试注入 OK 或其他生产状态，但本替身不访问 RTC。
 * @pre date_time 非 NULL；公共层负责校验时、分、秒范围。
 * @post 无论返回状态为何，s_datetime 的时分秒都被更新，日期字段保持不变。
 * @warning NULL date_time 会直接崩溃；返回值不代表真实 RTC 是否写入成功。
 * @note 这是测试替身，不配置 RTC 寄存器，也不模拟 HAL 忙、超时或错误状态。
 */
platform_status_t platform_rtc_backend_set_time(
  const platform_rtc_date_time_t *date_time
) {
  s_datetime.hour = date_time->hour;
  s_datetime.minute = date_time->minute;
  s_datetime.second = date_time->second;
  return s_set_time_status;
}

/**
 * @brief 将日期字段复制到测试日期时间快照并返回注入状态。
 * @param date_time 日期时间输入；读取 year、month、day、weekday，其他字段忽略；本替身不检查 NULL。
 * @return s_set_date_status 的当前值；测试注入 OK 或 ERROR，但本替身不访问 RTC。
 * @pre date_time 非 NULL；公共层负责校验年份、月份、日期和星期范围。
 * @post 无论返回状态为何，s_datetime 的日期字段都被更新，时间字段保持不变。
 * @warning NULL date_time 会直接崩溃；返回值不代表真实 RTC 是否写入成功。
 * @note 这是测试替身，不配置 RTC 寄存器，也不模拟 HAL 忙或超时状态。
 */
platform_status_t platform_rtc_backend_set_date(
  const platform_rtc_date_time_t *date_time
) {
  s_datetime.year = date_time->year;
  s_datetime.month = date_time->month;
  s_datetime.day = date_time->day;
  s_datetime.weekday = date_time->weekday;
  return s_set_date_status;
}

/**
 * @brief 将测试时间快照复制到调用方对象。
 * @param date_time 输出对象；本替身覆盖 hour、minute、second，不检查 NULL。
 * @retval PLATFORM_STATUS_OK 固定返回，并已复制时分秒。
 * @pre date_time 非 NULL 且指向可写对象；违反时解引用导致未定义行为。
 * @post s_datetime 不变，date_time 的时分秒被覆盖，其他日期字段保持不变。
 * @warning 测试替身不执行参数校验，NULL date_time 会直接崩溃而不是返回 ERROR。
 */
platform_status_t platform_rtc_backend_get_time(
  platform_rtc_date_time_t *date_time
) {
  date_time->hour = s_datetime.hour;
  date_time->minute = s_datetime.minute;
  date_time->second = s_datetime.second;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 将测试日期快照复制到调用方对象。
 * @param date_time 输出对象；本替身覆盖 year、month、day、weekday，不检查 NULL。
 * @retval PLATFORM_STATUS_OK 固定返回，并已复制日期字段。
 * @pre date_time 非 NULL 且指向可写对象；违反时解引用导致未定义行为。
 * @post s_datetime 不变，date_time 的日期字段被覆盖，时间字段保持不变。
 * @warning 测试替身不执行参数校验，NULL date_time 会直接崩溃而不是返回 ERROR。
 */
platform_status_t platform_rtc_backend_get_date(
  platform_rtc_date_time_t *date_time
) {
  date_time->year = s_datetime.year;
  date_time->month = s_datetime.month;
  date_time->day = s_datetime.day;
  date_time->weekday = s_datetime.weekday;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 返回测试注入的 RTC 闹钟配置状态。
 * @param alarm 每日闹钟时间；本替身忽略该参数，不读取字段，也不检查 NULL。
 * @return s_set_alarm_status 的当前值；测试注入 OK，但本替身不配置硬件闹钟。
 * @post s_set_alarm_status 和 alarm 指向的对象均不变，不修改 RTC 寄存器或回调状态。
 * @note 这是测试替身；生产后端还可能返回 ERROR、BUSY 或 TIMEOUT。
 */
platform_status_t platform_rtc_backend_set_alarm(
  const platform_rtc_alarm_config_t *alarm
) {
  (void)alarm;
  return s_set_alarm_status;
}

/**
 * @brief 模拟关闭 RTC 闹钟并固定报告成功。
 * @retval PLATFORM_STATUS_OK 固定返回，未关闭任何真实闹钟中断。
 * @post 不修改 RTC 寄存器、重装标志或全局测试状态。
 * @note 测试替身不会返回生产后端声明的 ERROR、BUSY 或 TIMEOUT 分支。
 */
platform_status_t platform_rtc_backend_disable_alarm(void) {
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 根据测试能力标志模拟启动唤醒定时器。
 * @param counter 重装计数值；本替身忽略该参数且不检查范围。
 * @param clock 唤醒时钟选择；本替身忽略该参数且不检查枚举值。
 * @retval PLATFORM_STATUS_OK s_has_wakeup 为 true；未配置真实唤醒定时器。
 * @retval PLATFORM_STATUS_UNSUPPORTED s_has_wakeup 为 false；表示测试替身不支持。
 * @post 不修改 RTC 寄存器，也不产生唤醒中断。
 * @note 测试替身不会返回生产后端声明的 BUSY 或 TIMEOUT 分支。
 */
platform_status_t platform_rtc_backend_start_wakeup(
  uint32_t counter,
  platform_rtc_wakeup_clock_t clock
) {
  (void)counter;
  (void)clock;
  return s_has_wakeup ? PLATFORM_STATUS_OK : PLATFORM_STATUS_UNSUPPORTED;
}

/**
 * @brief 根据测试能力标志模拟停止唤醒定时器。
 * @retval PLATFORM_STATUS_OK s_has_wakeup 为 true；未停止真实唤醒定时器。
 * @retval PLATFORM_STATUS_UNSUPPORTED s_has_wakeup 为 false；表示测试替身不支持。
 * @post 不修改 RTC 寄存器，也不取消任何唤醒中断。
 * @note 测试替身不会返回生产后端声明的 BUSY 或 TIMEOUT 分支。
 */
platform_status_t platform_rtc_backend_stop_wakeup(void) {
  return s_has_wakeup ? PLATFORM_STATUS_OK : PLATFORM_STATUS_UNSUPPORTED;
}

/**
 * @brief 记录 RTC 闹钟回调事件。
 * @param rtc 事件所属 RTC 实例；断言必须为 PLATFORM_RTC_1。
 * @param event 闹钟或唤醒事件；ALARM 增加闹钟计数，其他值增加唤醒计数。
 * @param context 注册上下文；断言必须等于 &s_alarm_count。
 * @pre 通过 Platform_RTC_RegisterCallback 注册，且 context 仍指向 s_alarm_count。
 * @post ALARM 使 s_alarm_count 加一；其他事件使 s_wakeup_count 加一。
 * @warning 实例或上下文不符合预期时 assert 会终止进程。
 * @note 测试通过同步分发调用本函数；真实平台回调在中断上下文执行，必须保持短小。
 */
static void alarm_callback(
  platform_rtc_instance_t rtc,
  platform_rtc_event_t event,
  void *context
) {
  assert(rtc == PLATFORM_RTC_1);
  assert(context == &s_alarm_count);
  if (event == PLATFORM_RTC_EVENT_ALARM) {
    s_alarm_count++;
  } else {
    s_wakeup_count++;
  }
}

/**
 * @brief 验证 RTC 参数校验、闹钟和事件分发。
 * @pre RTC 公共实现和本文件测试替身已链接；s_has_wakeup、三个状态变量可写。
 * @post 已覆盖非法年份、有效日期时间回读、日期写入失败错误位、闹钟设置、
 *       唤醒能力切换、回调注册去重和 ALARM/WAKEUP 分发；结束时 s_has_wakeup 为 true。
 * @warning 使用 assert 校验每一步；任一失败会调用 abort，进程不会正常返回。
 * @note 测试替身不访问 RTC 外设；出错分支只验证状态传播和错误快照位。
 */
static void test_rtc_contract(void) {
  s_has_wakeup = false;
  s_set_time_status = PLATFORM_STATUS_OK;
  s_set_date_status = PLATFORM_STATUS_OK;
  s_set_alarm_status = PLATFORM_STATUS_OK;

  assert(!Platform_RTC_HasWakeUpTimer(PLATFORM_RTC_1));

  platform_rtc_date_time_t invalid_datetime = {
    .year = 2100U,
    .month = 1U,
    .day = 1U,
    .weekday = 1U,
    .hour = 0U,
    .minute = 0U,
    .second = 0U,
  };
  assert(Platform_RTC_SetDateTime(PLATFORM_RTC_1, &invalid_datetime) ==
         PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_RTC_GetLastError(PLATFORM_RTC_1) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);

  const platform_rtc_date_time_t valid_datetime = {
    .year = 2026U,
    .month = 10U,
    .day = 24U,
    .weekday = 6U,
    .hour = 12U,
    .minute = 34U,
    .second = 56U,
  };
  assert(Platform_RTC_SetDateTime(PLATFORM_RTC_1, &valid_datetime) ==
         PLATFORM_STATUS_OK);

  platform_rtc_date_time_t readback = {0};
  assert(Platform_RTC_GetDateTime(PLATFORM_RTC_1, &readback) == PLATFORM_STATUS_OK);
  assert(readback.year == valid_datetime.year);
  assert(readback.month == valid_datetime.month);
  assert(readback.day == valid_datetime.day);
  assert(readback.weekday == valid_datetime.weekday);
  assert(readback.hour == valid_datetime.hour);
  assert(readback.minute == valid_datetime.minute);
  assert(readback.second == valid_datetime.second);

  s_set_date_status = PLATFORM_STATUS_ERROR;
  assert(Platform_RTC_SetDateTime(PLATFORM_RTC_1, &valid_datetime) ==
         PLATFORM_STATUS_ERROR);
  assert((Platform_RTC_GetLastError(PLATFORM_RTC_1) &
          PLATFORM_RTC_ERROR_DATE) != 0U);
  assert((Platform_RTC_GetLastError(PLATFORM_RTC_1) &
          PLATFORM_ERROR_HAL) != 0U);

  const platform_rtc_alarm_config_t alarm = {
    .hour = 1U,
    .minute = 2U,
    .second = 3U,
  };
  assert(Platform_RTC_SetAlarm(PLATFORM_RTC_1, &alarm) == PLATFORM_STATUS_OK);
  assert(Platform_RTC_StartWakeUpTimer(
           PLATFORM_RTC_1,
           10U,
           PLATFORM_RTC_WAKEUP_DIV16
         ) == PLATFORM_STATUS_UNSUPPORTED);
  assert((Platform_RTC_GetLastError(PLATFORM_RTC_1) &
          PLATFORM_ERROR_UNSUPPORTED) != 0U);

  const platform_rtc_callback_binding_t callbacks = {
    .alarm_cb = alarm_callback,
    .wakeup_cb = alarm_callback,
    .context = &s_alarm_count,
  };
  assert(Platform_RTC_RegisterCallback(PLATFORM_RTC_1, &callbacks) ==
         PLATFORM_STATUS_OK);
  assert(Platform_RTC_RegisterCallback(PLATFORM_RTC_1, &callbacks) ==
         PLATFORM_STATUS_BUSY);
  platform_rtc_dispatch_event(PLATFORM_RTC_EVENT_ALARM);
  platform_rtc_dispatch_event(PLATFORM_RTC_EVENT_WAKEUP);
  assert(s_alarm_count == 1U);
  assert(s_wakeup_count == 1U);

  s_has_wakeup = true;
  assert(Platform_RTC_HasWakeUpTimer(PLATFORM_RTC_1));
  assert(Platform_RTC_StartWakeUpTimer(
           PLATFORM_RTC_1,
           10U,
           PLATFORM_RTC_WAKEUP_DIV16
         ) == PLATFORM_STATUS_OK);
}

/**
 * @brief 运行本文件定义的契约测试并返回进程状态。
 * @retval 0 test_rtc_contract 的全部断言通过。
 * @pre RTC 测试替身已链接且全局观察状态可写。
 * @post 测试替身状态保留；没有动态内存、线程或真实 RTC 资源。
 * @warning 断言失败会直接终止进程，不会返回非零退出码。
 */
int main(void) {
  test_rtc_contract();
  return 0;
}
