/**
 * @file platform_rtc_backend.h
 * @brief STM32 系列公共 RTC 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_RTC_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_RTC_BACKEND_H

#include "platform_def.h"
#include "platform_rtc.h"
#include <stdbool.h>

/**
 * @brief 报告 RTC 后端是否支持唤醒定时器。
 */
bool platform_rtc_backend_has_wakeup_timer(void);

/**
 * @brief 写入二进制 RTC 时间。
 */
platform_status_t platform_rtc_backend_set_time(
  const platform_rtc_date_time_t *date_time
);

/**
 * @brief 写入二进制 RTC 日期。
 */
platform_status_t platform_rtc_backend_set_date(
  const platform_rtc_date_time_t *date_time
);

/**
 * @brief 读取二进制 RTC 时间。
 */
platform_status_t platform_rtc_backend_get_time(
  platform_rtc_date_time_t *date_time
);

/**
 * @brief 读取二进制 RTC 日期。
 */
platform_status_t platform_rtc_backend_get_date(
  platform_rtc_date_time_t *date_time
);

/**
 * @brief 写入 RTC 每日闹钟时间。
 */
platform_status_t platform_rtc_backend_set_alarm(
  const platform_rtc_alarm_config_t *alarm
);

/**
 * @brief 关闭 RTC 闹钟。
 */
platform_status_t platform_rtc_backend_disable_alarm(void);

/**
 * @brief 启动 RTC 唤醒定时器。
 */
platform_status_t platform_rtc_backend_start_wakeup(
  uint32_t counter,
  platform_rtc_wakeup_clock_t clock
);

/**
 * @brief 停止 RTC 唤醒定时器。
 */
platform_status_t platform_rtc_backend_stop_wakeup(void);

/**
 * @brief 把 RTC 闹钟或唤醒事件分发给已注册回调。
 */
void platform_rtc_dispatch_event(platform_rtc_event_t event);

/**
 * @brief 记录 RTC 软件错误位并保留硬件错误快照。
 */
void platform_rtc_record_error(uint32_t error);

#endif //LAYERED_EMBEDDED_C_PLATFORM_RTC_BACKEND_H
