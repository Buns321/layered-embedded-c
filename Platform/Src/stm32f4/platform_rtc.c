/**
 * @file platform_rtc.c
 * @brief STM32F4 公共 RTC 实现的 HAL 后端。
 * @ingroup platform
 */

#include "platform_hal_internal.h"
#include "platform_rtc_backend.h"
#include "rtc.h"
#include "stm32f4xx_hal.h"

/**
 * @brief 把平台唤醒时钟选择转换为 HAL 唤醒时钟宏。
 * @param clock 唤醒时钟分频或 CK_SPRE 位宽选择，必须是已定义的枚举值。
 * @retval 0U clock 超出已支持枚举范围，正常公共调用不会进入该分支。
 * @note 纯映射，不访问硬件、不分配内存，可在任务或中断上下文调用。
 */
static uint32_t platform_rtc_wakeup_clock(
  platform_rtc_wakeup_clock_t clock
) {
  switch (clock) {
    case PLATFORM_RTC_WAKEUP_DIV16:
      return RTC_WAKEUPCLOCK_RTCCLK_DIV16;
    case PLATFORM_RTC_WAKEUP_DIV8:
      return RTC_WAKEUPCLOCK_RTCCLK_DIV8;
    case PLATFORM_RTC_WAKEUP_DIV4:
      return RTC_WAKEUPCLOCK_RTCCLK_DIV4;
    case PLATFORM_RTC_WAKEUP_DIV2:
      return RTC_WAKEUPCLOCK_RTCCLK_DIV2;
    case PLATFORM_RTC_WAKEUP_CK_SPRE_16_BITS:
      return RTC_WAKEUPCLOCK_CK_SPRE_16BITS;
    case PLATFORM_RTC_WAKEUP_CK_SPRE_17_BITS:
      return RTC_WAKEUPCLOCK_CK_SPRE_17BITS;
    default:
      return 0U;
  }
}

/**
 * @brief 查询当前系列后端是否实现 RTC 唤醒定时器。
 * @retval true F4 后端提供唤醒定时器。
 * @retval false F1 后端固定不支持。
 * @note 只报告系列能力，不检查 hrtc 初始化状态，也不访问硬件。
 */
bool platform_rtc_backend_has_wakeup_timer(void) {
  return true;
}

/**
 * @brief 把二进制时分秒写入 RTC。
 * @param date_time 日期时间输入；只读取 hour、minute、second，其他字段忽略。
 * @retval PLATFORM_STATUS_OK HAL 已写入并确认时间。
 * @retval PLATFORM_STATUS_ERROR HAL 写入失败。
 * @retval PLATFORM_STATUS_BUSY RTC 尚未进入可配置状态。
 * @retval PLATFORM_STATUS_TIMEOUT F4 HAL 等待 RTC 初始化或写完成超时；F1 同类超时以 ERROR 返回。
 * @pre date_time 非 NULL，hour 为 0..23、minute 和 second 为 0..59；公共层已完成校验。
 * @post 成功时 RTC 时间更新，日期字段保持不变；失败时寄存器状态由 HAL 决定。
 * @warning F1 的闹钟回调内禁止调用 RTC 配置 API，以免与回调中的闹钟重装竞争。
 * @note 使用 24 小时制二进制格式；不分配内存、不加锁，调用方必须串行化 RTC 配置。
 */
platform_status_t platform_rtc_backend_set_time(
  const platform_rtc_date_time_t *date_time
) {
  RTC_TimeTypeDef time = {
    .Hours = date_time->hour,
    .Minutes = date_time->minute,
    .Seconds = date_time->second,
    .TimeFormat = RTC_HOURFORMAT_24,
    .DayLightSaving = RTC_DAYLIGHTSAVING_NONE,
    .StoreOperation = RTC_STOREOPERATION_RESET,
  };

  return platform_from_hal_status(
    HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN)
  );
}

/**
 * @brief 把二进制日期写入 RTC。
 * @param date_time 日期时间输入；读取 year、month、day、weekday，时间字段忽略。
 * @retval PLATFORM_STATUS_OK HAL 已写入并确认日期。
 * @retval PLATFORM_STATUS_ERROR HAL 写入失败。
 * @retval PLATFORM_STATUS_BUSY RTC 尚未进入可配置状态。
 * @retval PLATFORM_STATUS_TIMEOUT F4 HAL 等待 RTC 初始化或写完成超时；F1 同类超时以 ERROR 返回。
 * @pre date_time 非 NULL；year 为 2000..2099、month 为 1..12、day 对应该月、
 *       weekday 为 1..7；公共层已完成校验。
 * @post 成功时 RTC 日期更新，时间字段保持不变；年份写入寄存器时转换为 2000 偏移。
 *       失败时寄存器状态由 HAL 决定。
 * @warning F1 的闹钟回调内禁止调用 RTC 配置 API。
 * @note 不分配内存、不加锁，调用方必须串行化 RTC 配置。
 */
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

/**
 * @brief 从 RTC 读取二进制时分秒。
 * @param date_time 输出对象；成功时覆盖 hour、minute、second，不得为 NULL。
 * @retval PLATFORM_STATUS_OK HAL 已读取时间。
 * @retval PLATFORM_STATUS_ERROR HAL 读取失败。
 * @pre date_time 指向可写对象；公共层已完成参数校验。
 * @post 仅成功时覆盖时分秒；其他日期字段保持不变，失败时 date_time 也不被修改。
 * @note 使用 24 小时制二进制格式；不分配内存、不加锁，调用方必须串行化 RTC 访问。
 */
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

/**
 * @brief 从 RTC 读取二进制日期。
 * @param date_time 输出对象；成功时覆盖 year、month、day、weekday，不得为 NULL。
 * @retval PLATFORM_STATUS_OK HAL 已读取日期。
 * @retval PLATFORM_STATUS_ERROR HAL 读取失败。
 * @pre date_time 指向可写对象；公共层已完成参数校验。
 * @post 成功时 year 为 2000..2099，weekday 为 1..7；时间字段不变，失败时 date_time 也不被修改。
 * @note 与时间读取的日历快照顺序由公共层组合；本函数不分配内存、不加锁。
 */
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

/**
 * @brief 配置 RTC 每日闹钟 A 中断。
 * @param alarm 每日闹钟时间；读取 hour、minute、second，不得为 NULL。
 * @retval PLATFORM_STATUS_OK 闹钟 A 中断已启用。
 * @retval PLATFORM_STATUS_ERROR HAL 配置失败。
 * @retval PLATFORM_STATUS_BUSY RTC 尚未进入可配置状态。
 * @retval PLATFORM_STATUS_TIMEOUT F4 HAL 等待 RTC 初始化或写完成超时；F1 同类超时以 ERROR 返回。
 * @pre alarm 字段为 hour 0..23、minute 和 second 0..59；公共层已完成校验。
 * @post 成功时启用每日闹钟 A，只比较时分秒；F1 在回调中把计数器推进 86400 秒以重复，
 *       F4 使用日期屏蔽，不做软件重装。F1 用本次结果覆盖内部重装标志，失败时也会关闭软件重装判断。
 * @warning F1 的闹钟回调内禁止再次调用 RTC API。
 * @note 不分配内存、不加锁；调用方必须串行化闹钟配置。
 */
platform_status_t platform_rtc_backend_set_alarm(
  const platform_rtc_alarm_config_t *alarm
) {
  RTC_AlarmTypeDef hal_alarm = {
    .AlarmTime = {
      .Hours = alarm->hour,
      .Minutes = alarm->minute,
      .Seconds = alarm->second,
      .TimeFormat = RTC_HOURFORMAT_24,
      .DayLightSaving = RTC_DAYLIGHTSAVING_NONE,
      .StoreOperation = RTC_STOREOPERATION_RESET,
    },
    .AlarmMask = RTC_ALARMMASK_DATEWEEKDAY,
    .AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL,
    .AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE,
    .AlarmDateWeekDay = 1U,
    .Alarm = RTC_ALARM_A,
  };

  return platform_from_hal_status(
    HAL_RTC_SetAlarm_IT(&hrtc, &hal_alarm, RTC_FORMAT_BIN)
  );
}

/**
 * @brief 关闭 RTC 闹钟 A 中断。
 * @retval PLATFORM_STATUS_OK 闹钟 A 中断已关闭。
 * @retval PLATFORM_STATUS_ERROR HAL 关闭失败。
 * @retval PLATFORM_STATUS_BUSY RTC 尚未进入可配置状态。
 * @retval PLATFORM_STATUS_TIMEOUT HAL 等待写完成超时。
 * @post 成功时 F1 清除内部每日重装标志，F4 停用硬件闹钟 A；失败时不修改 F1 标志，
 *       硬件寄存器状态由 HAL 决定。
 * @note 不分配内存、不加锁；调用方必须串行化闹钟配置。
 */
platform_status_t platform_rtc_backend_disable_alarm(void) {
  return platform_from_hal_status(
    HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A)
  );
}

/**
 * @brief 按指定时钟源启动 RTC 唤醒定时器中断。
 * @param counter 以所选时钟节拍表示的重装计数值，范围 0..0xFFFF。
 * @param clock 唤醒时钟分频或 CK_SPRE 位宽选择。
 * @retval PLATFORM_STATUS_OK F4 已启用唤醒定时器中断。
 * @retval PLATFORM_STATUS_UNSUPPORTED F1 不支持唤醒定时器，且不访问硬件。
 * @retval PLATFORM_STATUS_BUSY RTC 尚未进入可配置状态。
 * @retval PLATFORM_STATUS_TIMEOUT HAL 等待写完成超时。
 * @pre counter 和 clock 已由公共层校验；F4 已通过 MX_RTC_Init() 初始化 RTC。
 * @post F4 成功时按所选时钟周期触发唤醒中断；失败时部分寄存器状态由 HAL 决定，F1 无状态变化。
 * @warning 实际周期取决于 RTC 时钟、分频和 (counter + 1) 重装语义，不能仅用 counter 推导秒数。
 * @note 不分配内存、不加锁；调用方必须串行化唤醒配置。
 */
platform_status_t platform_rtc_backend_start_wakeup(
  uint32_t counter,
  platform_rtc_wakeup_clock_t clock
) {
  return platform_from_hal_status(
    HAL_RTCEx_SetWakeUpTimer_IT(
      &hrtc,
      counter,
      platform_rtc_wakeup_clock(clock)
    )
  );
}

/**
 * @brief 停止 RTC 唤醒定时器中断。
 * @retval PLATFORM_STATUS_OK F4 已停用唤醒定时器。
 * @retval PLATFORM_STATUS_UNSUPPORTED F1 不支持唤醒定时器，且不访问硬件。
 * @retval PLATFORM_STATUS_BUSY RTC 尚未进入可配置状态。
 * @retval PLATFORM_STATUS_TIMEOUT HAL 等待写完成超时。
 * @post F4 成功时唤醒中断停止；失败时寄存器状态由 HAL 决定，F1 无状态变化。
 * @note 不分配内存、不加锁；调用方必须串行化唤醒配置。
 */
platform_status_t platform_rtc_backend_stop_wakeup(void) {
  return platform_from_hal_status(
    HAL_RTCEx_DeactivateWakeUpTimer(&hrtc)
  );
}

/**
 * @brief 转发 F4 RTC 闹钟 A 中断事件。
 * @param rtc_h HAL RTC 句柄；必须是 &hrtc，否则触发内部不变量断言。
 * @pre 由 STM32 HAL 在 RTC 闹钟 A 中断上下文调用；hrtc 已完成初始化。
 * @post 同步分发 PLATFORM_RTC_EVENT_ALARM；F4 不在此处重装闹钟。
 * @warning 位于中断上下文，不得阻塞、分配内存或执行长耗时操作。
 */
void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *rtc_h) {
  assert(rtc_h == &hrtc);
  platform_rtc_dispatch_event(PLATFORM_RTC_EVENT_ALARM);
}

/**
 * @brief 转发 F4 RTC 唤醒定时器中断事件。
 * @param rtc_h HAL RTC 句柄；必须是 &hrtc，否则触发内部不变量断言。
 * @pre 由 STM32 HAL 在 RTC 唤醒中断上下文调用；hrtc 已完成初始化。
 * @post 同步分发 PLATFORM_RTC_EVENT_WAKEUP。
 * @warning 位于中断上下文，不得阻塞、分配内存或执行长耗时操作。
 */
void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *rtc_h) {
  assert(rtc_h == &hrtc);
  platform_rtc_dispatch_event(PLATFORM_RTC_EVENT_WAKEUP);
}
