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

/**
 * @brief 以有界轮询等待 F1 RTC 寄存器写操作完成。
 * @retval true 在 PLATFORM_RTC_RTOFF_SPIN_LIMIT 次轮询内观察到 RTOFF。
 * @retval false 达到轮询上限仍未观察到 RTOFF。
 * @pre hrtc 已初始化，调用方独占 RTC 寄存器修改。
 * @post true 表示寄存器可继续写入；false 时本函数不修改任何寄存器。
 * @warning 忙等，耗时随 CPU 主频变化且没有毫秒级上限；在中断上下文中会延长 ISR。
 */
static bool platform_rtc_wait_for_ready(void) {
  for (uint32_t spin = 0U; spin < PLATFORM_RTC_RTOFF_SPIN_LIMIT; ++spin) {
    if ((hrtc.Instance->CRL & RTC_CRL_RTOFF) != 0U) {
      return true;
    }
  }
  return false;
}

/**
 * @brief 把 F1 的计数器闹钟推进一天，形成每日重复。
 * @retval true 新闹钟已写入并再次观察到寄存器就绪。
 * @retval false 写前等待超时，或写后未观察到就绪；已写入的 ALR 不回滚。
 * @pre hrtc 已初始化且当前闹钟 A 已启用；只应在 F1 RTC 闹钟回调的串行上下文调用。
 * @post 成功时 ALR 增加 PLATFORM_SECONDS_PER_DAY 个秒计数，32 位回绕按硬件语义处理。
 * @warning 写入期间短暂关闭 RTC 写保护；不分配内存，但会增加中断处理时间。
 */
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

/**
 * @brief 查询当前系列后端是否实现 RTC 唤醒定时器。
 * @retval true F4 后端提供唤醒定时器。
 * @retval false F1 后端固定不支持。
 * @note 只报告系列能力，不检查 hrtc 初始化状态，也不访问硬件。
 */
bool platform_rtc_backend_has_wakeup_timer(void) {
  return false;
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
    },
    .Alarm = RTC_ALARM_A,
  };

  platform_status_t status = platform_from_hal_status(
    HAL_RTC_SetAlarm_IT(&hrtc, &hal_alarm, RTC_FORMAT_BIN)
  );
  rtc_alarm_enabled = status == PLATFORM_STATUS_OK;
  return status;
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
  platform_status_t status = platform_from_hal_status(
    HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A)
  );
  if (status == PLATFORM_STATUS_OK) {
    rtc_alarm_enabled = false;
  }
  return status;
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
  (void)counter;
  (void)clock;
  return PLATFORM_STATUS_UNSUPPORTED;
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
  return PLATFORM_STATUS_UNSUPPORTED;
}

/**
 * @brief 处理 F1 RTC 闹钟 A 中断、重装每日闹钟并转发事件。
 * @param rtc_h HAL RTC 句柄；必须是 &hrtc，否则触发内部不变量断言。
 * @pre 由 STM32 HAL 在 RTC 闹钟 A 中断上下文调用；hrtc 已完成初始化。
 * @post 若每日闹钟处于启用状态，尝试把 ALR 推进 86400 秒；重装失败时记录
 *       PLATFORM_RTC_ERROR_ALARM | PLATFORM_ERROR_HAL，随后同步分发 ALARM 回调。
 * @warning 位于中断上下文，不得阻塞、分配内存或执行长耗时操作；回调内禁止再次调用 RTC API。
 */
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
