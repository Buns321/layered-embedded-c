/**
 * @file platform_rtc.c
 * @brief STM32F1 公共 RTC 实现的 HAL 后端。
 * @ingroup platform
 */

#include "platform_hal_internal.h"
#include "platform_rtc_backend.h"
#include "platform_validation_internal.h"
#include "rtc.h"
#include "stm32f1xx_hal.h"

#define PLATFORM_RTC_RTOFF_SPIN_LIMIT 100000UL

static volatile bool rtc_alarm_enabled;

static bool platform_rtc_wait_for_ready(void) {
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

bool platform_rtc_backend_has_wakeup_timer(void) {
  return false;
}

platform_status_t platform_rtc_backend_set_time(
  const platform_rtc_date_time_t *date_time
) {
  RTC_TimeTypeDef time = {
    .Hours = date_time->hour,
    .Minutes = date_time->minute,
    .Seconds = date_time->second,
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
    },
    .Alarm = RTC_ALARM_A,
  };

  platform_status_t status = platform_from_hal_status(
    HAL_RTC_SetAlarm_IT(&hrtc, &hal_alarm, RTC_FORMAT_BIN)
  );
  rtc_alarm_enabled = status == PLATFORM_STATUS_OK;
  return status;
}

platform_status_t platform_rtc_backend_disable_alarm(void) {
  platform_status_t status = platform_from_hal_status(
    HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A)
  );
  if (status == PLATFORM_STATUS_OK) {
    rtc_alarm_enabled = false;
  }
  return status;
}

platform_status_t platform_rtc_backend_start_wakeup(
  uint32_t counter,
  platform_rtc_wakeup_clock_t clock
) {
  (void)counter;
  (void)clock;
  return PLATFORM_STATUS_UNSUPPORTED;
}

platform_status_t platform_rtc_backend_stop_wakeup(void) {
  return PLATFORM_STATUS_UNSUPPORTED;
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *rtc_h) {
  assert(rtc_h == &hrtc);
  if (rtc_alarm_enabled && !platform_rtc_rearm_daily_alarm()) {
    platform_rtc_record_error(
      PLATFORM_RTC_ERROR_ALARM |
      PLATFORM_ERROR_HAL
    );
  }
  platform_rtc_dispatch_event(PLATFORM_RTC_EVENT_ALARM);
}
