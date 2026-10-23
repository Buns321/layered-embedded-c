//
// Created by Buns_ on 2026/10/23.
//

/**
 * @file platform_rtc.h
 * @brief HAL-free RTC date, time, alarm, and wake-up interface.
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_RTC_H
#define LAYERED_EMBEDDED_C_PLATFORM_RTC_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

#if !PLATFORM_ENABLE_RTC
#error "Platform RTC must be enabled before including platform_rtc.h"
#endif

/** @brief Available RTC instance. */
typedef enum {
  PLATFORM_RTC_1 = 0U,
} Platform_RTC_Instance;

/** @brief Calendar date and 24-hour time in binary form. */
typedef struct {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t weekday;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} Platform_RTC_DateTime;

/** @brief Daily alarm time in binary form. */
typedef struct {
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} Platform_RTC_AlarmConfig;

#if PLATFORM_RTC_HAS_WAKEUP_TIMER
/**
 * @brief RTC wake-up timer clock source.
 * @note The period is (counter + 1) ticks of the selected clock unless the
 *       reference manual specifies a different reload behavior.
 */
typedef enum {
  PLATFORM_RTC_WAKEUP_DIV16 = 0U,
  PLATFORM_RTC_WAKEUP_DIV8,
  PLATFORM_RTC_WAKEUP_DIV4,
  PLATFORM_RTC_WAKEUP_DIV2,
  PLATFORM_RTC_WAKEUP_CK_SPRE_16_BITS,
  PLATFORM_RTC_WAKEUP_CK_SPRE_17_BITS,
} Platform_RTC_WakeUpClock;

/** @brief Largest value accepted by the STM32F4 wake-up reload counter. */
#define PLATFORM_RTC_WAKEUP_COUNTER_MAX 0xFFFFU
#endif

/** @brief RTC error flags. */
typedef enum {
  PLATFORM_RTC_ERROR_NONE        = 0u,
  PLATFORM_RTC_ERROR_INVALID     = (1u << 0),
  PLATFORM_RTC_ERROR_TIME        = (1u << 1),
  PLATFORM_RTC_ERROR_DATE        = (1u << 2),
  PLATFORM_RTC_ERROR_ALARM       = (1u << 3),
  PLATFORM_RTC_ERROR_WAKEUP      = (1u << 4),
  PLATFORM_RTC_ERROR_UNKNOWN     = (1u << 7),
} Platform_RTC_Error;

typedef enum {
  PLATFORM_RTC_EVENT_ALARM = 0U,
#if PLATFORM_RTC_HAS_WAKEUP_TIMER
  PLATFORM_RTC_EVENT_WAKEUP,
#endif
} Platform_RTC_Event;

typedef void (*Platform_RTC_Callback)(
  Platform_RTC_Instance rtc,
  Platform_RTC_Event event,
  void *context
);

/** @brief RTC event callback owner. */
typedef struct {
  Platform_RTC_Callback alarm_cb;
#if PLATFORM_RTC_HAS_WAKEUP_TIMER
  Platform_RTC_Callback wakeup_cb;
#endif
  void *context;
} platform_rtc_callbacks_t;

/**
 * @brief Set the complete binary date and 24-hour time.
 * @param rtc RTC instance.
 * @param date_time Year must be 2000..2099, weekday must be 1..7 where Monday
 *                  is 1 and Sunday is 7. Invalid fields are rejected.
 */
Platform_StatusTypeDef Platform_RTC_SetDateTime(
  Platform_RTC_Instance rtc,
  const Platform_RTC_DateTime *date_time
);

/**
 * @brief Read the complete date and time.
 * @param rtc RTC instance.
 * @param date_time Destination date and time.
 * @note HAL reads time and date as one coherent calendar snapshot where
 *       supported.
 */
Platform_StatusTypeDef Platform_RTC_GetDateTime(
  Platform_RTC_Instance rtc,
  Platform_RTC_DateTime *date_time
);

/**
 * @brief Enable the daily alarm interrupt.
 * @param rtc RTC instance.
 * @param alarm Daily alarm time.
 * @note The alarm compares hour, minute, and second. Calendar-date matching is
 *       not part of the cross-family Platform contract.
 * @note STM32F1 re-arms the counter alarm for the following day from the
 *       interrupt callback. RTC APIs must not be called from that callback.
 */
Platform_StatusTypeDef Platform_RTC_SetAlarm(
  Platform_RTC_Instance rtc,
  const Platform_RTC_AlarmConfig *alarm
);

/**
 * @brief Disable the daily alarm interrupt.
 * @param rtc RTC instance.
 */
Platform_StatusTypeDef Platform_RTC_DisableAlarm(
  Platform_RTC_Instance rtc
);

#if PLATFORM_RTC_HAS_WAKEUP_TIMER
/**
 * @brief Start the RTC wake-up timer interrupt.
 * @param rtc RTC instance.
 * @param counter Reload counter in selected-clock ticks, 0..0xFFFF.
 * @param clock Wake-up timer clock source.
 * @note Wake-up timing is derived from the RTC clock; the effective period is
 *       documented by the selected clock and counter reload semantics.
 */
Platform_StatusTypeDef Platform_RTC_StartWakeUpTimer(
  Platform_RTC_Instance rtc,
  uint32_t counter,
  Platform_RTC_WakeUpClock clock
);

/**
 * @brief Stop the RTC wake-up timer.
 * @param rtc RTC instance.
 */
Platform_StatusTypeDef Platform_RTC_StopWakeUpTimer(
  Platform_RTC_Instance rtc
);
#endif

/**
 * @brief Read the latest Platform RTC operation error snapshot.
 * @param rtc RTC instance.
 */
uint32_t Platform_RTC_GetLastError(Platform_RTC_Instance rtc);

/**
 * @brief Register RTC event callbacks.
 * @param rtc RTC instance.
 * @param callbacks Callback set to copy.
 * @note Callbacks run in interrupt context. Call once during initialization.
 */
void Platform_RTC_RegisterCallbacks(
  Platform_RTC_Instance rtc,
  const platform_rtc_callbacks_t *callbacks
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_RTC_H
