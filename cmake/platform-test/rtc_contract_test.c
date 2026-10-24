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

bool platform_rtc_backend_has_wakeup_timer(void) {
  return s_has_wakeup;
}

platform_status_t platform_rtc_backend_set_time(
  const platform_rtc_date_time_t *date_time
) {
  s_datetime.hour = date_time->hour;
  s_datetime.minute = date_time->minute;
  s_datetime.second = date_time->second;
  return s_set_time_status;
}

platform_status_t platform_rtc_backend_set_date(
  const platform_rtc_date_time_t *date_time
) {
  s_datetime.year = date_time->year;
  s_datetime.month = date_time->month;
  s_datetime.day = date_time->day;
  s_datetime.weekday = date_time->weekday;
  return s_set_date_status;
}

platform_status_t platform_rtc_backend_get_time(
  platform_rtc_date_time_t *date_time
) {
  date_time->hour = s_datetime.hour;
  date_time->minute = s_datetime.minute;
  date_time->second = s_datetime.second;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_rtc_backend_get_date(
  platform_rtc_date_time_t *date_time
) {
  date_time->year = s_datetime.year;
  date_time->month = s_datetime.month;
  date_time->day = s_datetime.day;
  date_time->weekday = s_datetime.weekday;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_rtc_backend_set_alarm(
  const platform_rtc_alarm_config_t *alarm
) {
  (void)alarm;
  return s_set_alarm_status;
}

platform_status_t platform_rtc_backend_disable_alarm(void) {
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_rtc_backend_start_wakeup(
  uint32_t counter,
  platform_rtc_wakeup_clock_t clock
) {
  (void)counter;
  (void)clock;
  return s_has_wakeup ? PLATFORM_STATUS_OK : PLATFORM_STATUS_UNSUPPORTED;
}

platform_status_t platform_rtc_backend_stop_wakeup(void) {
  return s_has_wakeup ? PLATFORM_STATUS_OK : PLATFORM_STATUS_UNSUPPORTED;
}

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

int main(void) {
  test_rtc_contract();
  return 0;
}
