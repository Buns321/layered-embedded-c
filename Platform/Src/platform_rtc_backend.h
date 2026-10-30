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
 * @brief 查询当前系列后端是否实现 RTC 唤醒定时器。
 * @retval true F4 后端提供唤醒定时器。
 * @retval false F1 后端固定不支持。
 * @note 只报告系列能力，不检查 hrtc 初始化状态，也不访问硬件。
 */
bool platform_rtc_backend_has_wakeup_timer(void);

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
);

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
);

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
);

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
);

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
);

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
platform_status_t platform_rtc_backend_disable_alarm(void);

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
);

/**
 * @brief 停止 RTC 唤醒定时器中断。
 * @retval PLATFORM_STATUS_OK F4 已停用唤醒定时器。
 * @retval PLATFORM_STATUS_UNSUPPORTED F1 不支持唤醒定时器，且不访问硬件。
 * @retval PLATFORM_STATUS_BUSY RTC 尚未进入可配置状态。
 * @retval PLATFORM_STATUS_TIMEOUT HAL 等待写完成超时。
 * @post F4 成功时唤醒中断停止；失败时寄存器状态由 HAL 决定，F1 无状态变化。
 * @note 不分配内存、不加锁；调用方必须串行化唤醒配置。
 */
platform_status_t platform_rtc_backend_stop_wakeup(void);

/**
 * @brief 把 RTC 闹钟或唤醒事件分发给已注册回调。
 * @param event 事件类型；ALARM 选择 alarm_cb，WAKEUP 选择 wakeup_cb。
 * @post 对应回调非 NULL 时，以 PLATFORM_RTC_1、event 和注册时保存的 context 调用一次；
 *       事件非法或回调为 NULL 时不调用任何函数。
 * @warning 通常在中断上下文执行；禁止在回调内阻塞或分配内存。
 * @note 回调副本在注册时保存，context 仍归调用方所有；本函数不分配内存。
 */
void platform_rtc_dispatch_event(platform_rtc_event_t event);

/**
 * @brief 记录 RTC 软件错误位并保留硬件错误快照。
 * @param error `platform_rtc_error_t` 与 `PLATFORM_ERROR_*` 的组合位掩码；0 表示不新增位。
 * @post 错误快照按位或更新，既有位不会被清除；不访问 RTC 硬件。
 * @warning volatile 读改写不是原子操作，任务与中断并发记录时可能丢失个别位。
 * @note 可由中断上下文调用；不分配内存、不阻塞。
 */
void platform_rtc_record_error(uint32_t error);

#endif //LAYERED_EMBEDDED_C_PLATFORM_RTC_BACKEND_H
