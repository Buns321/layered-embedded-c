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

Platform_StatusTypeDef platform_rtc_backend_set_time(
  const Platform_RTC_DateTime *date_time
);

Platform_StatusTypeDef platform_rtc_backend_set_date(
  const Platform_RTC_DateTime *date_time
);

Platform_StatusTypeDef platform_rtc_backend_get_time(
  Platform_RTC_DateTime *date_time
);

Platform_StatusTypeDef platform_rtc_backend_get_date(
  Platform_RTC_DateTime *date_time
);

Platform_StatusTypeDef platform_rtc_backend_set_alarm(
  const Platform_RTC_AlarmConfig *alarm
);

Platform_StatusTypeDef platform_rtc_backend_disable_alarm(void);

Platform_StatusTypeDef platform_rtc_backend_start_wakeup(
  uint32_t counter,
  Platform_RTC_WakeUpClock clock
);

Platform_StatusTypeDef platform_rtc_backend_stop_wakeup(void);

void platform_rtc_dispatch_event(Platform_RTC_Event event);

void platform_rtc_record_error(uint32_t error);

#endif //LAYERED_EMBEDDED_C_PLATFORM_RTC_BACKEND_H
