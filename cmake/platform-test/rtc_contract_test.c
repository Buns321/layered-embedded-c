#include "platform_rtc_backend.h"
#include "platform_rtc.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static bool s_has_wakeup;
static Platform_StatusTypeDef s_set_time_status;
static Platform_StatusTypeDef s_set_date_status;
static Platform_StatusTypeDef s_set_alarm_status;
static Platform_RTC_DateTime s_datetime;
static uint32_t s_alarm_count;
static uint32_t s_wakeup_count;

bool platform_rtc_backend_has_wakeup_timer(void) {
  return s_has_wakeup;
}

Platform_StatusTypeDef platform_rtc_backend_set_time(
  const Platform_RTC_DateTime *date_time
) {
  s_datetime.hour = date_time->hour;
  s_datetime.minute = date_time->minute;
  s_datetime.second = date_time->second;
  return s_set_time_status;
}

Platform_StatusTypeDef platform_rtc_backend_set_date(
  const Platform_RTC_DateTime *date_time
) {
  s_datetime.year = date_time->year;
  s_datetime.month = date_time->month;
  s_datetime.day = date_time->day;
  s_datetime.weekday = date_time->weekday;
  return s_set_date_status;
}

Platform_StatusTypeDef platform_rtc_backend_get_time(
  Platform_RTC_DateTime *date_time
) {
  date_time->hour = s_datetime.hour;
  date_time->minute = s_datetime.minute;
  date_time->second = s_datetime.second;
  return Platform_OK;
}

Platform_StatusTypeDef platform_rtc_backend_get_date(
  Platform_RTC_DateTime *date_time
) {
  date_time->year = s_datetime.year;
  date_time->month = s_datetime.month;
  date_time->day = s_datetime.day;
  date_time->weekday = s_datetime.weekday;
  return Platform_OK;
}

Platform_StatusTypeDef platform_rtc_backend_set_alarm(
  const Platform_RTC_AlarmConfig *alarm
) {
  (void)alarm;
  return s_set_alarm_status;
}

Platform_StatusTypeDef platform_rtc_backend_disable_alarm(void) {
  return Platform_OK;
}

Platform_StatusTypeDef platform_rtc_backend_start_wakeup(
  uint32_t counter,
  Platform_RTC_WakeUpClock clock
) {
  (void)counter;
  (void)clock;
  return s_has_wakeup ? Platform_OK : Platform_UNSUPPORTED;
}

Platform_StatusTypeDef platform_rtc_backend_stop_wakeup(void) {
  return s_has_wakeup ? Platform_OK : Platform_UNSUPPORTED;
}

static void alarm_callback(
  Platform_RTC_Instance rtc,
  Platform_RTC_Event event,
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
  s_set_time_status = Platform_OK;
  s_set_date_status = Platform_OK;
  s_set_alarm_status = Platform_OK;

  assert(!Platform_RTC_HasWakeUpTimer(PLATFORM_RTC_1));

  Platform_RTC_DateTime invalid_datetime = {
    .year = 2100U,
    .month = 1U,
    .day = 1U,
    .weekday = 1U,
    .hour = 0U,
    .minute = 0U,
    .second = 0U,
  };
  assert(Platform_RTC_SetDateTime(PLATFORM_RTC_1, &invalid_datetime) ==
         Platform_INVALID_ARGUMENT);
  assert((Platform_RTC_GetLastError(PLATFORM_RTC_1) &
          PLATFORM_COMMON_ERROR_PARAMETER) != 0U);

  const Platform_RTC_DateTime valid_datetime = {
    .year = 2026U,
    .month = 10U,
    .day = 24U,
    .weekday = 6U,
    .hour = 12U,
    .minute = 34U,
    .second = 56U,
  };
  assert(Platform_RTC_SetDateTime(PLATFORM_RTC_1, &valid_datetime) ==
         Platform_OK);

  Platform_RTC_DateTime readback = {0};
  assert(Platform_RTC_GetDateTime(PLATFORM_RTC_1, &readback) == Platform_OK);
  assert(readback.year == valid_datetime.year);
  assert(readback.month == valid_datetime.month);
  assert(readback.day == valid_datetime.day);
  assert(readback.weekday == valid_datetime.weekday);
  assert(readback.hour == valid_datetime.hour);
  assert(readback.minute == valid_datetime.minute);
  assert(readback.second == valid_datetime.second);

  s_set_date_status = Platform_ERROR;
  assert(Platform_RTC_SetDateTime(PLATFORM_RTC_1, &valid_datetime) ==
         Platform_ERROR);
  assert((Platform_RTC_GetLastError(PLATFORM_RTC_1) &
          PLATFORM_RTC_ERROR_DATE) != 0U);
  assert((Platform_RTC_GetLastError(PLATFORM_RTC_1) &
          PLATFORM_COMMON_ERROR_HAL) != 0U);

  const Platform_RTC_AlarmConfig alarm = {
    .hour = 1U,
    .minute = 2U,
    .second = 3U,
  };
  assert(Platform_RTC_SetAlarm(PLATFORM_RTC_1, &alarm) == Platform_OK);
  assert(Platform_RTC_StartWakeUpTimer(
           PLATFORM_RTC_1,
           10U,
           PLATFORM_RTC_WAKEUP_DIV16
         ) == Platform_UNSUPPORTED);
  assert((Platform_RTC_GetLastError(PLATFORM_RTC_1) &
          PLATFORM_COMMON_ERROR_UNSUPPORTED) != 0U);

  const platform_rtc_callbacks_t callbacks = {
    .alarm_cb = alarm_callback,
    .wakeup_cb = alarm_callback,
    .context = &s_alarm_count,
  };
  assert(Platform_RTC_RegisterCallbacks(PLATFORM_RTC_1, &callbacks) ==
         Platform_OK);
  assert(Platform_RTC_RegisterCallbacks(PLATFORM_RTC_1, &callbacks) ==
         Platform_BUSY);
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
         ) == Platform_OK);
}

int main(void) {
  test_rtc_contract();
  return 0;
}
