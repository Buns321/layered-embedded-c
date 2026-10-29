/**
 * @file platform_rtc_common.c
 * @brief 与芯片系列无关的 RTC 参数校验、错误管理和回调分发。
 * @ingroup platform
 */

#include "platform_error_internal.h"
#include "platform_rtc_backend.h"
#include <stddef.h>

static volatile uint32_t rtc_error;
static platform_rtc_callback_binding_t rtc_cb;
static bool rtc_cb_registered;

/**
 * @brief 判断年份是否为闰年。
 */
static bool platform_rtc_is_leap_year(uint16_t year) {
  return ((year % 4U) == 0U &&
          ((year % 100U) != 0U || (year % 400U) == 0U));
}

/**
 * @brief 返回指定月份的天数。
 */
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

/**
 * @brief 校验 RTC 日期时间范围。
 */
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

/**
 * @brief 校验 RTC 时间字段范围。
 */
static bool platform_rtc_time_valid(
  const platform_rtc_alarm_config_t *alarm
) {
  return alarm != NULL &&
         alarm->hour <= 23U &&
         alarm->minute <= 59U &&
         alarm->second <= 59U;
}

/**
 * @brief 把 HAL RTC 后端结果转换为公共状态。
 */
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

/**
 * @brief 记录 RTC 软件错误位并保留硬件错误快照。
 */
void platform_rtc_record_error(uint32_t error) {
  platform_error_record(&rtc_error, error);
}

/**
 * @brief 把 RTC 闹钟或唤醒事件分发给已注册回调。
 */
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

/**
 * @brief 查询当前系列是否支持 RTC 唤醒定时器。
 * @param rtc RTC 实例。
 * @return 支持返回 true；F1 或不支持实例返回 false。
 */
bool Platform_RTC_HasWakeUpTimer(platform_rtc_instance_t rtc) {
  return rtc == PLATFORM_RTC_1 &&
         platform_rtc_backend_has_wakeup_timer();
}

/**
 * @brief 设置完整的二进制日期与 24 小时时间。
 * @param rtc RTC 实例。
 * @param date_time 年份必须在 2000..2099 内，weekday 必须在 1..7 内，
 *                  其中周一到周日依次为 1..7。非法字段会被拒绝。
 */
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

/**
 * @brief 读取完整日期和时间。
 * @param rtc RTC 实例。
 * @param date_time 目标日期和时间。
 * @note 在硬件支持时，HAL 会把日期和时间作为一个一致的日历快照读取。
 */
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

/**
 * @brief 启用每日闹钟中断。
 * @param rtc RTC 实例。
 * @param alarm 每日闹钟时间。
 * @note 闹钟只比较时、分、秒。跨系列 Platform 契约不包含日历日期匹配。
 * @note STM32F1 会在中断回调中把计数器闹钟重装到第二天。禁止在该回调中
 *       调用 RTC API。
 */
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

/**
 * @brief 禁用每日闹钟中断。
 * @param rtc RTC 实例。
 */
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

/**
 * @brief 启动 RTC 唤醒定时器中断。
 * @param rtc RTC 实例。
 * @param counter 以所选时钟节拍表示的重装计数器，范围 0..0xFFFF。
 * @param clock 唤醒定时器时钟源。
 * @retval PLATFORM_STATUS_UNSUPPORTED 当前系列不支持唤醒定时器。
 * @note 唤醒时序由 RTC 时钟推导；实际周期取决于所选时钟和计数器重装语义。
 */
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

/**
 * @brief 停止 RTC 唤醒定时器。
 * @param rtc RTC 实例。
 */
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

/**
 * @brief 读取最近一次 Platform RTC 操作错误快照。
 * @param rtc RTC 实例。
 * @return `platform_rtc_error_t` 与 `PLATFORM_ERROR_*` 位掩码的组合。
 * @note 读取不清除快照。
 */
uint32_t Platform_RTC_GetLastError(platform_rtc_instance_t rtc) {
  if (rtc != PLATFORM_RTC_1) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return rtc_error;
}

/**
 * @brief 注册 RTC 事件回调。
 * @param rtc RTC 实例。
 * @param callbacks 待复制的回调集合。
 * @note 回调在中断上下文执行。应在初始化阶段调用一次。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该实例已注册回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或回调集合非法。
 */
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
