/**
 * @file platform_rtc.c
 * @brief STM32F4 公共 RTC 实现的 HAL 后端。
 * @ingroup platform
 */

#include "platform_hal_internal.h"
#include "platform_rtc_backend.h"
#include "rtc.h"
#include "stm32f4xx_hal.h"

static uint32_t platform_rtc_wakeup_clock(
  platform_rtc_wakeup_clock_t clock
) {
  switch (clock) {
    case PLATFORM_RTC_WAKEUP_DIV16:
      return RTC_WAKEUPCLOCK_RTCCLK_DIV16;
    case PLATFORM_RTC_WAKEUP_DIV8:
      return RTC_WAKEUPCLOCK_RTCCLK_DIV8;
    case PLATFORM_RTC_WAKEUP_DIV4:
      return RTC_WAKEUPCLOCK_RTCCLK_DIV4;
    case PLATFORM_RTC_WAKEUP_DIV2:
      return RTC_WAKEUPCLOCK_RTCCLK_DIV2;
    case PLATFORM_RTC_WAKEUP_CK_SPRE_16_BITS:
      return RTC_WAKEUPCLOCK_CK_SPRE_16BITS;
    case PLATFORM_RTC_WAKEUP_CK_SPRE_17_BITS:
      return RTC_WAKEUPCLOCK_CK_SPRE_17BITS;
    default:
      return 0U;
  }
}

bool platform_rtc_backend_has_wakeup_timer(void) {
  return true;
}

platform_status_t platform_rtc_backend_set_time(
  const platform_rtc_date_time_t *date_time
) {
  RTC_TimeTypeDef time = {
    .Hours = date_time->hour,
    .Minutes = date_time->minute,
    .Seconds = date_time->second,
    .TimeFormat = RTC_HOURFORMAT_24,
    .DayLightSaving = RTC_DAYLIGHTSAVING_NONE,
    .StoreOperation = RTC_STOREOPERATION_RESET,
  };

  return platform_from_hal_status(
    HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN)
  );
}

platform_status_t platform_rtc_backend_set_date(
  const platform_rtc_date_time_t *date_time
) {
  RTC_DateTypeDef date = {
    .WeekDay = date_time->weekday,
    .Month = date_time->month,
    .Date = date_time->day,
    .Year = (uint8_t)(date_time->year - 2000U),
  };

  return platform_from_hal_status(
    HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN)
  );
}

platform_status_t platform_rtc_backend_get_time(
  platform_rtc_date_time_t *date_time
) {
  RTC_TimeTypeDef time;
  platform_status_t status = platform_from_hal_status(
    HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN)
  );
  if (status == PLATFORM_STATUS_OK) {
    date_time->hour = (uint8_t)time.Hours;
    date_time->minute = (uint8_t)time.Minutes;
    date_time->second = (uint8_t)time.Seconds;
  }
  return status;
}

platform_status_t platform_rtc_backend_get_date(
  platform_rtc_date_time_t *date_time
) {
  RTC_DateTypeDef date;
  platform_status_t status = platform_from_hal_status(
    HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN)
  );
  if (status == PLATFORM_STATUS_OK) {
    date_time->year = (uint16_t)(2000U + date.Year);
    date_time->month = (uint8_t)date.Month;
    date_time->day = (uint8_t)date.Date;
    date_time->weekday = (uint8_t)date.WeekDay;
  }
  return status;
}

platform_status_t platform_rtc_backend_set_alarm(
  const platform_rtc_alarm_config_t *alarm
) {
  RTC_AlarmTypeDef hal_alarm = {
    .AlarmTime = {
      .Hours = alarm->hour,
      .Minutes = alarm->minute,
      .Seconds = alarm->second,
      .TimeFormat = RTC_HOURFORMAT_24,
      .DayLightSaving = RTC_DAYLIGHTSAVING_NONE,
      .StoreOperation = RTC_STOREOPERATION_RESET,
    },
    .AlarmMask = RTC_ALARMMASK_DATEWEEKDAY,
    .AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL,
    .AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE,
    .AlarmDateWeekDay = 1U,
    .Alarm = RTC_ALARM_A,
  };

  return platform_from_hal_status(
    HAL_RTC_SetAlarm_IT(&hrtc, &hal_alarm, RTC_FORMAT_BIN)
  );
}

platform_status_t platform_rtc_backend_disable_alarm(void) {
  return platform_from_hal_status(
    HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A)
  );
}

platform_status_t platform_rtc_backend_start_wakeup(
  uint32_t counter,
  platform_rtc_wakeup_clock_t clock
) {
  return platform_from_hal_status(
    HAL_RTCEx_SetWakeUpTimer_IT(
      &hrtc,
      counter,
      platform_rtc_wakeup_clock(clock)
    )
  );
}

platform_status_t platform_rtc_backend_stop_wakeup(void) {
  return platform_from_hal_status(
    HAL_RTCEx_DeactivateWakeUpTimer(&hrtc)
  );
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *rtc_h) {
  assert(rtc_h == &hrtc);
  platform_rtc_dispatch_event(PLATFORM_RTC_EVENT_ALARM);
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *rtc_h) {
  assert(rtc_h == &hrtc);
  platform_rtc_dispatch_event(PLATFORM_RTC_EVENT_WAKEUP);
}
