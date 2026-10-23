/**
 * @file platform_rtc_common.c
 * @brief Family-independent RTC validation, errors and callback dispatch.
 */

#include "platform_error_internal.h"
#include "platform_rtc_backend.h"
#include <stddef.h>

static volatile uint32_t rtc_error;
static platform_rtc_callback_binding_t rtc_cb;
static bool rtc_cb_registered;

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
  const platform_rtc_date_time_t *date_time
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
  const platform_rtc_alarm_config_t *alarm
) {
  return alarm != NULL &&
         alarm->hour <= 23U &&
         alarm->minute <= 59U &&
         alarm->second <= 59U;
}

static platform_status_t platform_rtc_finish_backend(
  platform_status_t status,
  uint32_t operation_error
) {
  if (status != PLATFORM_STATUS_OK) {
    platform_rtc_record_error(
      platform_error_from_status(status) | operation_error
    );
  }
  return status;
}

void platform_rtc_record_error(uint32_t error) {
  platform_error_record(&rtc_error, error);
}

void platform_rtc_dispatch_event(platform_rtc_event_t event) {
  platform_rtc_callback_t callback = NULL;
  switch (event) {
    case PLATFORM_RTC_EVENT_ALARM:
      callback = rtc_cb.alarm_cb;
      break;
    case PLATFORM_RTC_EVENT_WAKEUP:
      callback = rtc_cb.wakeup_cb;
      break;
    default:
      return;
  }

  if (callback != NULL) {
    callback(PLATFORM_RTC_1, event, rtc_cb.context);
  }
}

bool Platform_RTC_HasWakeUpTimer(platform_rtc_instance_t rtc) {
  return rtc == PLATFORM_RTC_1 &&
         platform_rtc_backend_has_wakeup_timer();
}

platform_status_t Platform_RTC_SetDateTime(
  platform_rtc_instance_t rtc,
  const platform_rtc_date_time_t *date_time
) {
  platform_error_reset(&rtc_error);
  if (rtc != PLATFORM_RTC_1 || !platform_rtc_datetime_valid(date_time)) {
    platform_rtc_record_error(
      PLATFORM_RTC_ERROR_INVALID |
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  platform_status_t status =
    platform_rtc_backend_set_time(date_time);
  if (status != PLATFORM_STATUS_OK) {
    return platform_rtc_finish_backend(
      status,
      PLATFORM_RTC_ERROR_TIME
    );
  }

  status = platform_rtc_backend_set_date(date_time);
  return platform_rtc_finish_backend(status, PLATFORM_RTC_ERROR_DATE);
}

platform_status_t Platform_RTC_GetDateTime(
  platform_rtc_instance_t rtc,
  platform_rtc_date_time_t *date_time
) {
  platform_error_reset(&rtc_error);
  if (rtc != PLATFORM_RTC_1 || date_time == NULL) {
    platform_rtc_record_error(
      PLATFORM_RTC_ERROR_INVALID |
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  platform_status_t status =
    platform_rtc_backend_get_time(date_time);
  if (status != PLATFORM_STATUS_OK) {
    return platform_rtc_finish_backend(
      status,
      PLATFORM_RTC_ERROR_TIME
    );
  }

  status = platform_rtc_backend_get_date(date_time);
  return platform_rtc_finish_backend(status, PLATFORM_RTC_ERROR_DATE);
}

platform_status_t Platform_RTC_SetAlarm(
  platform_rtc_instance_t rtc,
  const platform_rtc_alarm_config_t *alarm
) {
  platform_error_reset(&rtc_error);
  if (rtc != PLATFORM_RTC_1 || !platform_rtc_time_valid(alarm)) {
    platform_rtc_record_error(
      PLATFORM_RTC_ERROR_INVALID |
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  return platform_rtc_finish_backend(
    platform_rtc_backend_set_alarm(alarm),
    PLATFORM_RTC_ERROR_ALARM
  );
}

platform_status_t Platform_RTC_DisableAlarm(
  platform_rtc_instance_t rtc
) {
  platform_error_reset(&rtc_error);
  if (rtc != PLATFORM_RTC_1) {
    platform_rtc_record_error(
      PLATFORM_RTC_ERROR_INVALID |
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  return platform_rtc_finish_backend(
    platform_rtc_backend_disable_alarm(),
    PLATFORM_RTC_ERROR_ALARM
  );
}

platform_status_t Platform_RTC_StartWakeUpTimer(
  platform_rtc_instance_t rtc,
  uint32_t counter,
  platform_rtc_wakeup_clock_t clock
) {
  platform_error_reset(&rtc_error);
  if (
    rtc != PLATFORM_RTC_1 ||
    counter > PLATFORM_RTC_WAKEUP_COUNTER_MAX ||
    clock > PLATFORM_RTC_WAKEUP_CK_SPRE_17_BITS
  ) {
    platform_rtc_record_error(
      PLATFORM_RTC_ERROR_INVALID |
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (!platform_rtc_backend_has_wakeup_timer()) {
    platform_rtc_record_error(PLATFORM_ERROR_UNSUPPORTED);
    return PLATFORM_STATUS_UNSUPPORTED;
  }

  return platform_rtc_finish_backend(
    platform_rtc_backend_start_wakeup(counter, clock),
    PLATFORM_RTC_ERROR_WAKEUP
  );
}

platform_status_t Platform_RTC_StopWakeUpTimer(
  platform_rtc_instance_t rtc
) {
  platform_error_reset(&rtc_error);
  if (rtc != PLATFORM_RTC_1) {
    platform_rtc_record_error(
      PLATFORM_RTC_ERROR_INVALID |
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (!platform_rtc_backend_has_wakeup_timer()) {
    platform_rtc_record_error(PLATFORM_ERROR_UNSUPPORTED);
    return PLATFORM_STATUS_UNSUPPORTED;
  }

  return platform_rtc_finish_backend(
    platform_rtc_backend_stop_wakeup(),
    PLATFORM_RTC_ERROR_WAKEUP
  );
}

uint32_t Platform_RTC_GetLastError(platform_rtc_instance_t rtc) {
  if (rtc != PLATFORM_RTC_1) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return rtc_error;
}

platform_status_t Platform_RTC_RegisterCallback(
  platform_rtc_instance_t rtc,
  const platform_rtc_callback_binding_t *callbacks
) {
  if (rtc != PLATFORM_RTC_1 || callbacks == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (rtc_cb_registered) {
    return PLATFORM_STATUS_BUSY;
  }

  rtc_cb = *callbacks;
  rtc_cb_registered = true;
  return PLATFORM_STATUS_OK;
}
