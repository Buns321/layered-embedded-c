//
// Created by Buns_ on 2026/10/23.
//

#include "platform_rtc.h"
#include "platform_hal_internal.h"
#include "rtc.h"
#include "stm32f4xx_hal.h"
#include <assert.h>
#include <stdbool.h>

static uint32_t rtc_error = PLATFORM_RTC_ERROR_NONE;
static bool rtc_cb_registered;
static platform_rtc_callbacks_t rtc_cb;

static bool platform_rtc_is_leap_year(uint16_t year) {
  return ((year % 4U) == 0U &&
          ((year % 100U) != 0U || (year % 400U) == 0U));
}

static uint8_t platform_rtc_days_in_month(uint16_t year, uint8_t month) {
  static const uint8_t days[12] = {
    31U, 28U, 31U, 30U, 31U, 30U,
    31U, 31U, 30U, 31U, 30U, 31U,
  };
  if (month == 2U && platform_rtc_is_leap_year(year)) {
    return 29U;
  }
  return days[month - 1U];
}

static bool platform_rtc_datetime_valid(
  const Platform_RTC_DateTime *date_time
) {
  return date_time != NULL &&
         date_time->year >= 2000U && date_time->year <= 2099U &&
         date_time->month >= 1U && date_time->month <= 12U &&
         date_time->day >= 1U &&
         date_time->day <=
           platform_rtc_days_in_month(date_time->year, date_time->month) &&
         date_time->weekday >= 1U && date_time->weekday <= 7U &&
         date_time->hour <= 23U &&
         date_time->minute <= 59U &&
         date_time->second <= 59U;
}

static bool platform_rtc_time_valid(
  const Platform_RTC_AlarmConfig *alarm
) {
  return alarm != NULL &&
         alarm->hour <= 23U &&
         alarm->minute <= 59U &&
         alarm->second <= 59U;
}

static Platform_StatusTypeDef platform_rtc_status(
  HAL_StatusTypeDef hal_status,
  uint32_t operation_error
) {
  if (hal_status != HAL_OK) {
    rtc_error |= operation_error;
  }
  return platform_from_hal_status(hal_status);
}

Platform_StatusTypeDef Platform_RTC_SetDateTime(
  Platform_RTC_Instance rtc,
  const Platform_RTC_DateTime *date_time
) {
  assert(rtc == PLATFORM_RTC_1);
  (void)rtc;
  rtc_error = PLATFORM_RTC_ERROR_NONE;
  if (!platform_rtc_datetime_valid(date_time)) {
    rtc_error = PLATFORM_RTC_ERROR_INVALID;
    return Platform_ERROR;
  }

  RTC_TimeTypeDef time = {
    .Hours = date_time->hour,
    .Minutes = date_time->minute,
    .Seconds = date_time->second,
    .TimeFormat = RTC_HOURFORMAT_24,
    .DayLightSaving = RTC_DAYLIGHTSAVING_NONE,
    .StoreOperation = RTC_STOREOPERATION_RESET,
  };
  RTC_DateTypeDef date = {
    .WeekDay = date_time->weekday,
    .Month = date_time->month,
    .Date = date_time->day,
    .Year = (uint8_t)(date_time->year - 2000U),
  };

  HAL_StatusTypeDef status = HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN);
  if (status != HAL_OK) {
    return platform_rtc_status(status, PLATFORM_RTC_ERROR_TIME);
  }
  status = HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN);
  return platform_rtc_status(status, PLATFORM_RTC_ERROR_DATE);
}

Platform_StatusTypeDef Platform_RTC_GetDateTime(
  Platform_RTC_Instance rtc,
  Platform_RTC_DateTime *date_time
) {
  assert(rtc == PLATFORM_RTC_1);
  (void)rtc;
  assert(date_time != NULL);
  rtc_error = PLATFORM_RTC_ERROR_NONE;

  RTC_TimeTypeDef time;
  RTC_DateTypeDef date;
  HAL_StatusTypeDef status = HAL_RTC_GetTime(
    &hrtc,
    &time,
    RTC_FORMAT_BIN
  );
  if (status != HAL_OK) {
    return platform_rtc_status(status, PLATFORM_RTC_ERROR_TIME);
  }
  status = HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);
  if (status != HAL_OK) {
    return platform_rtc_status(status, PLATFORM_RTC_ERROR_DATE);
  }

  date_time->year = (uint16_t)(2000U + date.Year);
  date_time->month = (uint8_t)date.Month;
  date_time->day = (uint8_t)date.Date;
  date_time->weekday = (uint8_t)date.WeekDay;
  date_time->hour = (uint8_t)time.Hours;
  date_time->minute = (uint8_t)time.Minutes;
  date_time->second = (uint8_t)time.Seconds;
  return Platform_OK;
}

Platform_StatusTypeDef Platform_RTC_SetAlarm(
  Platform_RTC_Instance rtc,
  const Platform_RTC_AlarmConfig *alarm
) {
  assert(rtc == PLATFORM_RTC_1);
  (void)rtc;
  rtc_error = PLATFORM_RTC_ERROR_NONE;
  if (!platform_rtc_time_valid(alarm)) {
    rtc_error = PLATFORM_RTC_ERROR_INVALID;
    return Platform_ERROR;
  }

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

  return platform_rtc_status(
    HAL_RTC_SetAlarm_IT(&hrtc, &hal_alarm, RTC_FORMAT_BIN),
    PLATFORM_RTC_ERROR_ALARM
  );
}

Platform_StatusTypeDef Platform_RTC_DisableAlarm(
  Platform_RTC_Instance rtc
) {
  assert(rtc == PLATFORM_RTC_1);
  (void)rtc;
  rtc_error = PLATFORM_RTC_ERROR_NONE;
  return platform_rtc_status(
    HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A),
    PLATFORM_RTC_ERROR_ALARM
  );
}

static uint32_t platform_rtc_wakeup_clock(
  Platform_RTC_WakeUpClock clock
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
      assert(0);
      return RTC_WAKEUPCLOCK_RTCCLK_DIV16;
  }
}

Platform_StatusTypeDef Platform_RTC_StartWakeUpTimer(
  Platform_RTC_Instance rtc,
  uint32_t counter,
  Platform_RTC_WakeUpClock clock
) {
  assert(rtc == PLATFORM_RTC_1);
  (void)rtc;
  rtc_error = PLATFORM_RTC_ERROR_NONE;
  if (counter > PLATFORM_RTC_WAKEUP_COUNTER_MAX) {
    rtc_error = PLATFORM_RTC_ERROR_INVALID;
    return Platform_ERROR;
  }
  return platform_rtc_status(
    HAL_RTCEx_SetWakeUpTimer_IT(
      &hrtc,
      counter,
      platform_rtc_wakeup_clock(clock)
    ),
    PLATFORM_RTC_ERROR_WAKEUP
  );
}

Platform_StatusTypeDef Platform_RTC_StopWakeUpTimer(
  Platform_RTC_Instance rtc
) {
  assert(rtc == PLATFORM_RTC_1);
  (void)rtc;
  rtc_error = PLATFORM_RTC_ERROR_NONE;
  return platform_rtc_status(
    HAL_RTCEx_DeactivateWakeUpTimer(&hrtc),
    PLATFORM_RTC_ERROR_WAKEUP
  );
}

uint32_t Platform_RTC_GetLastError(Platform_RTC_Instance rtc) {
  assert(rtc == PLATFORM_RTC_1);
  (void)rtc;
  return rtc_error;
}

void Platform_RTC_RegisterCallbacks(
  Platform_RTC_Instance rtc,
  const platform_rtc_callbacks_t *callbacks
) {
  assert(rtc == PLATFORM_RTC_1);
  (void)rtc;
  assert(callbacks != NULL);
  assert(!rtc_cb_registered);
  rtc_cb = *callbacks;
  rtc_cb_registered = true;
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc_handle) {
  assert(hrtc_handle == &hrtc);
  (void)hrtc_handle;
  if (rtc_cb.alarm_cb != NULL) {
    rtc_cb.alarm_cb(PLATFORM_RTC_1, PLATFORM_RTC_EVENT_ALARM, rtc_cb.context);
  }
}

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc_handle) {
  assert(hrtc_handle == &hrtc);
  (void)hrtc_handle;
  if (rtc_cb.wakeup_cb != NULL) {
    rtc_cb.wakeup_cb(
      PLATFORM_RTC_1,
      PLATFORM_RTC_EVENT_WAKEUP,
      rtc_cb.context
    );
  }
}
