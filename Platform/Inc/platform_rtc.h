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
 * @param rtc RTC 实例；当前仅 PLATFORM_RTC_1 可能受支持。
 * @retval true 当前系列后端实现唤醒定时器。
 * @retval false rtc 非法，或当前后端（F1）不支持唤醒定时器。
 * @pre rtc 可取自公共枚举或来自调用方配置；本函数不要求 RTC 已初始化。
 * @post 只读查询，不修改错误快照、不访问 RTC 硬件。
 * @note 不分配内存、不阻塞；可并发调用。
 */
bool Platform_RTC_HasWakeUpTimer(platform_rtc_instance_t rtc);

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
);

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
);

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
);

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
);

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
);

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
);

/**
 * @brief 读取最近一次 Platform RTC 操作错误快照。
 * @param rtc RTC 实例；当前仅接受 PLATFORM_RTC_1。
 * @retval PLATFORM_ERROR_INVALID_ARGUMENT rtc 非法。
 * @post 合法实例返回当前快照；非法实例不访问共享快照。
 * @note 只读，不清除快照；后续公共 RTC 操作开始时会清零快照。
 * @note 不访问硬件、不分配内存；并发读写同一快照须由调用方串行化。
 */
uint32_t Platform_RTC_GetLastError(platform_rtc_instance_t rtc);

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
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_RTC_H
