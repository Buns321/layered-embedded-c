//
// 由 Buns_ 创建于 2026/10/23。
//

#include "platform_rtc.h"
#include "platform_hal_internal.h"
#include "platform_validation_internal.h"
#include "rtc.h"
#include "stm32f1xx_hal.h"
#include <assert.h>
#include <stdbool.h>

static volatile uint32_t rtc_error = PLATFORM_RTC_ERROR_NONE;
static bool rtc_cb_registered;
static volatile bool rtc_alarm_enabled;
static platform_rtc_callbacks_t rtc_cb;

#define PLATFORM_RTC_RTOFF_SPIN_LIMIT 100000UL

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

static bool platform_rtc_wait_for_ready(void) {
  /* 避免在 RTC 闹钟 ISR 中进入 HAL_GetTick 的超时处理路径。 */
  for (uint32_t spin = 0U; spin < PLATFORM_RTC_RTOFF_SPIN_LIMIT; ++spin) {
    if ((hrtc.Instance->CRL & RTC_CRL_RTOFF) != 0U) {
      return true;
    }
  }
  return false;
}

static bool platform_rtc_rearm_daily_alarm(void) {
  const uint32_t current_alarm =
    ((uint32_t)hrtc.Instance->ALRH << 16U) |
    (hrtc.Instance->ALRL & 0xFFFFUL);
  const uint32_t next_alarm =
    platform_rtc_next_daily_alarm(current_alarm);

  if (!platform_rtc_wait_for_ready()) {
    return false;
  }

  __HAL_RTC_WRITEPROTECTION_DISABLE(&hrtc);
  WRITE_REG(hrtc.Instance->ALRH, next_alarm >> 16U);
  WRITE_REG(hrtc.Instance->ALRL, next_alarm & 0xFFFFUL);
  __HAL_RTC_WRITEPROTECTION_ENABLE(&hrtc);

  return platform_rtc_wait_for_ready();
}

Platform_StatusTypeDef Platform_RTC_SetDateTime(
  Platform_RTC_Instance rtc,
  const Platform_RTC_DateTime *date_time
) {
  rtc_error = PLATFORM_RTC_ERROR_NONE;
  if (rtc != PLATFORM_RTC_1 || !platform_rtc_datetime_valid(date_time)) {
    rtc_error = PLATFORM_RTC_ERROR_INVALID;
    return Platform_ERROR;
  }

  RTC_TimeTypeDef time = {
    .Hours = date_time->hour,
    .Minutes = date_time->minute,
    .Seconds = date_time->second,
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
  rtc_error = PLATFORM_RTC_ERROR_NONE;
  if (rtc != PLATFORM_RTC_1 || date_time == NULL) {
    rtc_error = PLATFORM_RTC_ERROR_INVALID;
    return Platform_ERROR;
  }

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
  rtc_error = PLATFORM_RTC_ERROR_NONE;
  if (rtc != PLATFORM_RTC_1 || !platform_rtc_time_valid(alarm)) {
    rtc_error = PLATFORM_RTC_ERROR_INVALID;
    return Platform_ERROR;
  }

  RTC_AlarmTypeDef hal_alarm = {
    .AlarmTime = {
      .Hours = alarm->hour,
      .Minutes = alarm->minute,
      .Seconds = alarm->second,
    },
    .Alarm = RTC_ALARM_A,
  };
  rtc_alarm_enabled = true;
  Platform_StatusTypeDef status = platform_rtc_status(
    HAL_RTC_SetAlarm_IT(&hrtc, &hal_alarm, RTC_FORMAT_BIN),
    PLATFORM_RTC_ERROR_ALARM
  );
  if (status != Platform_OK) {
    rtc_alarm_enabled = false;
  }
  return status;
}

Platform_StatusTypeDef Platform_RTC_DisableAlarm(
  Platform_RTC_Instance rtc
) {
  rtc_error = PLATFORM_RTC_ERROR_NONE;
  if (rtc != PLATFORM_RTC_1) {
    rtc_error = PLATFORM_RTC_ERROR_INVALID;
    return Platform_ERROR;
  }
  Platform_StatusTypeDef status = platform_rtc_status(
    HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A),
    PLATFORM_RTC_ERROR_ALARM
  );
  if (status == Platform_OK) {
    rtc_alarm_enabled = false;
  }
  return status;
}

uint32_t Platform_RTC_GetLastError(Platform_RTC_Instance rtc) {
  return rtc == PLATFORM_RTC_1
           ? rtc_error
           : PLATFORM_RTC_ERROR_UNKNOWN;
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
  if (rtc_alarm_enabled && !platform_rtc_rearm_daily_alarm()) {
    rtc_error |= PLATFORM_RTC_ERROR_ALARM;
  }
  if (rtc_cb.alarm_cb != NULL) {
    rtc_cb.alarm_cb(PLATFORM_RTC_1, PLATFORM_RTC_EVENT_ALARM, rtc_cb.context);
  }
}
