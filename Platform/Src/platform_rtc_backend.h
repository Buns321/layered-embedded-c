/**
 * @file platform_rtc_backend.h
 * @brief Private STM32-family backend for the common RTC implementation.
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_RTC_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_RTC_BACKEND_H

#include "platform_def.h"
#include "platform_rtc.h"
#include <stdbool.h>

bool platform_rtc_backend_has_wakeup_timer(void);

platform_status_t platform_rtc_backend_set_time(
  const platform_rtc_date_time_t *date_time
);

platform_status_t platform_rtc_backend_set_date(
  const platform_rtc_date_time_t *date_time
);

platform_status_t platform_rtc_backend_get_time(
  platform_rtc_date_time_t *date_time
);

platform_status_t platform_rtc_backend_get_date(
  platform_rtc_date_time_t *date_time
);

platform_status_t platform_rtc_backend_set_alarm(
  const platform_rtc_alarm_config_t *alarm
);

platform_status_t platform_rtc_backend_disable_alarm(void);

platform_status_t platform_rtc_backend_start_wakeup(
  uint32_t counter,
  platform_rtc_wakeup_clock_t clock
);

platform_status_t platform_rtc_backend_stop_wakeup(void);

void platform_rtc_dispatch_event(platform_rtc_event_t event);

void platform_rtc_record_error(uint32_t error);

#endif //LAYERED_EMBEDDED_C_PLATFORM_RTC_BACKEND_H
