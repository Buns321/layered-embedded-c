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
 * @param year 公历年号；本函数不限制范围，公共日期校验会将其限定为 2000..2099。
 * @retval true 年份能被 4 整除，但不能被 100 整除，或能被 400 整除。
 * @retval false 其他年份。
 * @post 纯函数，不修改参数或模块状态。
 * @note 内部辅助函数；不分配内存、不阻塞，可并发调用。
 */
static bool platform_rtc_is_leap_year(uint16_t year) {
  return ((year % 4U) == 0U &&
          ((year % 100U) != 0U || (year % 400U) == 0U));
}

/**
 * @brief 返回指定月份的天数。
 * @param year 公历年号；用于判断二月是否包含 29 日。
 * @param month 月份，范围 1..12；调用方保证该前置条件。
 * @return 指定月份的天数，范围 28..31。
 * @pre month 必须为 1..12；超出范围会越界访问静态表，禁止由公共 API 直接传入未校验值。
 * @post 纯函数，不修改参数或模块状态。
 * @note 内部辅助函数；不分配内存、不阻塞，可并发调用。
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
 * @param date_time 待校验的日期时间；允许为 NULL。
 * @retval true 年份、日期、星期和时间字段均在公共契约范围内。
 * @retval false date_time 为 NULL，或任一字段越界、日期与该月不匹配。
 * @post 纯函数；不修改输入对象或模块状态。
 * @note 日期按公历校验，weekday 仅检查 1..7，不验证其与日期是否一致；
 *       内部辅助函数，不分配内存、不阻塞。
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
 * @param alarm 待校验的每日闹钟时间；允许为 NULL。
 * @retval true alarm 非 NULL，且 hour 为 0..23、minute 和 second 为 0..59。
 * @retval false alarm 为 NULL 或任一时间字段越界。
 * @post 纯函数；不修改输入对象或模块状态。
 * @note 内部辅助函数；不访问 RTC 硬件、不分配内存、不阻塞。
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
 * @param status 后端返回的公共操作状态。
 * @param operation_error 失败时附加的 RTC 操作错误位，例如 PLATFORM_RTC_ERROR_TIME。
 * @return 原样返回 status，便于调用方直接转交。
 * @post status 非 OK 时，将 platform_error_from_status(status) 与 operation_error
 *       合并写入错误快照；成功时不修改快照。
 * @note 内部辅助函数；错误快照的并发限制由 platform_rtc_record_error() 决定。
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
 * @param error `platform_rtc_error_t` 与 `PLATFORM_ERROR_*` 的组合位掩码；0 表示不新增位。
 * @post 错误快照按位或更新，既有位不会被清除；不访问 RTC 硬件。
 * @warning volatile 读改写不是原子操作，任务与中断并发记录时可能丢失个别位。
 * @note 可由中断上下文调用；不分配内存、不阻塞。
 */
void platform_rtc_record_error(uint32_t error) {
  platform_error_record(&rtc_error, error);
}

/**
 * @brief 把 RTC 闹钟或唤醒事件分发给已注册回调。
 * @param event 事件类型；ALARM 选择 alarm_cb，WAKEUP 选择 wakeup_cb。
 * @post 对应回调非 NULL 时，以 PLATFORM_RTC_1、event 和注册时保存的 context 调用一次；
 *       事件非法或回调为 NULL 时不调用任何函数。
 * @warning 通常在中断上下文执行；禁止在回调内阻塞或分配内存。
 * @note 回调副本在注册时保存，context 仍归调用方所有；本函数不分配内存。
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
 * @param rtc RTC 实例；当前仅 PLATFORM_RTC_1 可能受支持。
 * @retval true 当前系列后端实现唤醒定时器。
 * @retval false rtc 非法，或当前后端（F1）不支持唤醒定时器。
 * @pre rtc 可取自公共枚举或来自调用方配置；本函数不要求 RTC 已初始化。
 * @post 只读查询，不修改错误快照、不访问 RTC 硬件。
 * @note 不分配内存、不阻塞；可并发调用。
 */
bool Platform_RTC_HasWakeUpTimer(platform_rtc_instance_t rtc) {
  return rtc == PLATFORM_RTC_1 &&
         platform_rtc_backend_has_wakeup_timer();
}

/**
 * @brief 设置完整的二进制日期与 24 小时时间。
 * @param rtc RTC 实例；当前仅接受 PLATFORM_RTC_1。
 * @param date_time 源日期时间；year 为 2000..2099、month 为 1..12、day 必须匹配该月，
 *        weekday 为 1..7（周一至周日），hour 为 0..23、minute 和 second 为 0..59；
 *        不得为 NULL。
 * @retval PLATFORM_STATUS_OK 时间和日期均已写入。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或字段范围非法；硬件不变。
 * @retval PLATFORM_STATUS_BUSY RTC 尚未进入可配置状态；寄存器状态由 HAL 决定。
 * @retval PLATFORM_STATUS_TIMEOUT HAL 等待初始化或写完成超时；寄存器状态由 HAL 决定。
 * @retval PLATFORM_STATUS_ERROR 时间或日期写入失败；寄存器状态由 HAL 决定。
 * @pre date_time 指向的对象在调用期间有效，且调用方已串行化 RTC 配置。
 * @post 实现先写时间再写日期；若时间成功而日期失败，时间可能已改变且日期保持原值。
 * @warning 在 F1 闹钟回调执行期间不得调用本函数；其他上下文中的 RTC 配置也不得并发。
 * @note 不分配内存、不加锁；调用开始时清除错误快照，后端失败时合并 RTC 与 PLATFORM_ERROR 位。
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
 * @param rtc RTC 实例；当前仅接受 PLATFORM_RTC_1。
 * @param date_time 输出对象；不得为 NULL，调用期间必须可写。
 * @retval PLATFORM_STATUS_OK 日期和时间均已读取。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或输出指针非法；date_time 不变。
 * @retval PLATFORM_STATUS_BUSY RTC 尚未进入可读取状态；date_time 可能部分更新。
 * @retval PLATFORM_STATUS_TIMEOUT HAL 读取超时；date_time 可能部分更新。
 * @retval PLATFORM_STATUS_ERROR 时间或日期读取失败；date_time 可能部分更新。
 * @pre date_time 指向可写对象，且调用方已串行化 RTC 访问。
 * @post 成功时覆盖全部字段；若时间成功而日期失败，时分秒可能已更新且日期字段保持原值。
 * @note 实现按 HAL 要求先调用 HAL_RTC_GetTime() 再调用 HAL_RTC_GetDate()，
 *       以读取同一日历快照；不分配内存、不加锁。
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
 * @param rtc RTC 实例；当前仅接受 PLATFORM_RTC_1。
 * @param alarm 每日闹钟时间；hour 为 0..23、minute 和 second 为 0..59，不得为 NULL。
 * @retval PLATFORM_STATUS_OK 每日闹钟 A 中断已启用。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或时间字段非法；硬件不变。
 * @retval PLATFORM_STATUS_BUSY RTC 尚未进入可配置状态；寄存器状态由 HAL 决定。
 * @retval PLATFORM_STATUS_TIMEOUT HAL 等待初始化或写完成超时；寄存器状态由 HAL 决定。
 * @retval PLATFORM_STATUS_ERROR HAL 配置失败；寄存器状态由 HAL 决定。
 * @pre alarm 指向的对象在调用期间有效，且调用方已串行化 RTC 配置。
 * @post 成功时仅按时分秒触发每日闹钟；F1 在中断中把计数器推进 86400 秒以重复，
 *       F4 使用日期屏蔽且不进行软件重装。
 * @warning F1 的闹钟回调内禁止调用任何 RTC API，以免与回调中的闹钟重装竞争。
 * @note 不分配内存、不加锁；跨系列 Platform 契约不包含日历日期匹配。
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
 * @param rtc RTC 实例；当前仅接受 PLATFORM_RTC_1。
 * @retval PLATFORM_STATUS_OK 闹钟 A 中断已关闭。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT rtc 非法；硬件不变。
 * @retval PLATFORM_STATUS_BUSY RTC 尚未进入可配置状态；寄存器状态由 HAL 决定。
 * @retval PLATFORM_STATUS_TIMEOUT HAL 等待写完成超时；寄存器状态由 HAL 决定。
 * @retval PLATFORM_STATUS_ERROR HAL 关闭失败；寄存器状态由 HAL 决定。
 * @pre 调用方已串行化 RTC 配置，且未处于 F1 闹钟回调中。
 * @post 成功时停用每日闹钟；F1 清除内部每日重装标志，失败时该标志保持不变。
 * @note 不分配内存、不加锁；调用开始时清除错误快照。
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
 * @param rtc RTC 实例；当前仅接受 PLATFORM_RTC_1。
 * @param counter 以所选时钟节拍表示的重装计数器，范围 0..0xFFFF。
 * @param clock 唤醒定时器时钟源；必须是 platform_rtc_wakeup_clock_t 的枚举值。
 * @retval PLATFORM_STATUS_OK 当前系列已启用唤醒定时器中断。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、counter 或 clock 非法；硬件不变。
 * @retval PLATFORM_STATUS_UNSUPPORTED 当前系列（F1）不支持唤醒定时器，且不访问硬件。
 * @retval PLATFORM_STATUS_BUSY RTC 尚未进入可配置状态；寄存器状态由 HAL 决定。
 * @retval PLATFORM_STATUS_TIMEOUT HAL 等待写完成超时；寄存器状态由 HAL 决定。
 * @retval PLATFORM_STATUS_ERROR HAL 启动失败；寄存器状态由 HAL 决定。
 * @pre 调用方已串行化 RTC 唤醒配置；F4 的 RTC 已由 CubeMX 初始化。
 * @post 成功时按所选时钟和重装计数器周期触发唤醒中断。
 * @warning 实际周期取决于 RTC 时钟、分频和 (counter + 1) 重装语义，不能仅用 counter
 *          推导秒数；重新配置前应停止已有唤醒定时器。
 * @note 不分配内存、不加锁；调用开始时清除错误快照。
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
 * @param rtc RTC 实例；当前仅接受 PLATFORM_RTC_1。
 * @retval PLATFORM_STATUS_OK 唤醒定时器已停止。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT rtc 非法；硬件不变。
 * @retval PLATFORM_STATUS_UNSUPPORTED 当前系列（F1）不支持唤醒定时器，且不访问硬件。
 * @retval PLATFORM_STATUS_BUSY RTC 尚未进入可配置状态；寄存器状态由 HAL 决定。
 * @retval PLATFORM_STATUS_TIMEOUT HAL 等待写完成超时；寄存器状态由 HAL 决定。
 * @retval PLATFORM_STATUS_ERROR HAL 停止失败；寄存器状态由 HAL 决定。
 * @pre 调用方已串行化 RTC 唤醒配置。
 * @post 成功时唤醒中断停止；未启动时是否返回成功由 HAL 决定。
 * @note 不分配内存、不加锁；调用开始时清除错误快照。
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
 * @param rtc RTC 实例；当前仅接受 PLATFORM_RTC_1。
 * @retval PLATFORM_ERROR_INVALID_ARGUMENT rtc 非法。
 * @post 合法实例返回当前快照；非法实例不访问共享快照。
 * @note 只读，不清除快照；后续公共 RTC 操作开始时会清零快照。
 * @note 不访问硬件、不分配内存；并发读写同一快照须由调用方串行化。
 */
uint32_t Platform_RTC_GetLastError(platform_rtc_instance_t rtc) {
  if (rtc != PLATFORM_RTC_1) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return rtc_error;
}

/**
 * @brief 注册 RTC 事件回调。
 * @param rtc RTC 实例；当前仅接受 PLATFORM_RTC_1。
 * @param callbacks 待复制的回调集合；不得为 NULL，成员为 NULL 表示忽略对应事件。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该实例已注册回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或回调集合非法。
 * @pre callbacks 指向的对象在调用期间有效；每个实例仅允许成功注册一次。
 * @post 成功时复制回调指针和 context，后续闹钟或唤醒事件按事件类型调用对应回调。
 * @warning 回调在中断上下文执行，不得阻塞、分配内存或调用 F1 的 RTC 配置 API。
 * @note 不分配内存、不加锁；应在初始化阶段完成注册，context 的生命周期由调用方维护。
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
