//
// 由 Buns_ 创建于 2026/10/23。
//

/**
 * @file platform_rtc.h
 * @brief 不依赖 HAL 类型的 RTC 日期、时间、闹钟与唤醒接口。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_RTC_H
#define LAYERED_EMBEDDED_C_PLATFORM_RTC_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

#if !PLATFORM_ENABLE_RTC
#error "Platform RTC must be enabled before including platform_rtc.h"
#endif

/** @brief 可用的 RTC 实例。 */
typedef enum platform_rtc_instance_e {
  PLATFORM_RTC_1 = 0U,
} platform_rtc_instance_t;

/** @brief 二进制形式的日历日期与 24 小时时间。 */
typedef struct platform_rtc_date_time_s {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t weekday;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} platform_rtc_date_time_t;

/** @brief 二进制形式的每日闹钟时间。 */
typedef struct platform_rtc_alarm_config_s {
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
} platform_rtc_alarm_config_t;

/**
 * @brief RTC 唤醒定时器时钟源。
 * @note 除非参考手册规定了不同的重装行为，周期为所选时钟的
 *       (counter + 1) 个时钟节拍。
 */
typedef enum platform_rtc_wakeup_clock_e {
  PLATFORM_RTC_WAKEUP_DIV16 = 0U,
  PLATFORM_RTC_WAKEUP_DIV8,
  PLATFORM_RTC_WAKEUP_DIV4,
  PLATFORM_RTC_WAKEUP_DIV2,
  PLATFORM_RTC_WAKEUP_CK_SPRE_16_BITS,
  PLATFORM_RTC_WAKEUP_CK_SPRE_17_BITS,
} platform_rtc_wakeup_clock_t;

/** @brief STM32F4 唤醒重装计数器可接受的最大值。 */
#define PLATFORM_RTC_WAKEUP_COUNTER_MAX 0xFFFFU

/** @brief RTC 错误标志。 */
typedef enum platform_rtc_error_e {
  PLATFORM_RTC_ERROR_NONE        = 0u,
  PLATFORM_RTC_ERROR_INVALID     = (1u << 0),
  PLATFORM_RTC_ERROR_TIME        = (1u << 1),
  PLATFORM_RTC_ERROR_DATE        = (1u << 2),
  PLATFORM_RTC_ERROR_ALARM       = (1u << 3),
  PLATFORM_RTC_ERROR_WAKEUP      = (1u << 4),
  PLATFORM_RTC_ERROR_UNKNOWN     = (1u << 7),
} platform_rtc_error_t;

typedef enum platform_rtc_event_e {
  PLATFORM_RTC_EVENT_ALARM = 0U,
  PLATFORM_RTC_EVENT_WAKEUP,
} platform_rtc_event_t;

typedef void (*platform_rtc_callback_t)(
  platform_rtc_instance_t rtc,
  platform_rtc_event_t event,
  void *context
);

/** @brief RTC 事件回调所有者。 */
typedef struct platform_rtc_callback_binding_s {
  platform_rtc_callback_t alarm_cb;
  platform_rtc_callback_t wakeup_cb;
  void *context;
} platform_rtc_callback_binding_t;

/**
 * @brief 查询当前系列是否支持 RTC 唤醒定时器。
 * @param rtc RTC 实例。
 * @return 支持返回 true；F1 或不支持实例返回 false。
 */
bool Platform_RTC_HasWakeUpTimer(platform_rtc_instance_t rtc);

/**
 * @brief 设置完整的二进制日期与 24 小时时间。
 * @param rtc RTC 实例。
 * @param date_time 年份必须在 2000..2099 内，weekday 必须在 1..7 内，
 *                  其中周一到周日依次为 1..7。非法字段会被拒绝。
 */
platform_status_t Platform_RTC_SetDateTime(
  platform_rtc_instance_t rtc,
  const platform_rtc_date_time_t *date_time
);

/**
 * @brief 读取完整日期和时间。
 * @param rtc RTC 实例。
 * @param date_time 目标日期和时间。
 * @note 在硬件支持时，HAL 会把日期和时间作为一个一致的日历快照读取。
 */
platform_status_t Platform_RTC_GetDateTime(
  platform_rtc_instance_t rtc,
  platform_rtc_date_time_t *date_time
);

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
);

/**
 * @brief 禁用每日闹钟中断。
 * @param rtc RTC 实例。
 */
platform_status_t Platform_RTC_DisableAlarm(
  platform_rtc_instance_t rtc
);

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
);

/**
 * @brief 停止 RTC 唤醒定时器。
 * @param rtc RTC 实例。
 */
platform_status_t Platform_RTC_StopWakeUpTimer(
  platform_rtc_instance_t rtc
);

/**
 * @brief 读取最近一次 Platform RTC 操作错误快照。
 * @param rtc RTC 实例。
 * @return `platform_rtc_error_t` 与 `PLATFORM_ERROR_*` 位掩码的组合。
 * @note 读取不清除快照。
 */
uint32_t Platform_RTC_GetLastError(platform_rtc_instance_t rtc);

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
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_RTC_H
