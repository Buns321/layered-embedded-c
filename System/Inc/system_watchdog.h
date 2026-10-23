//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file system_watchdog.h
 * @brief IWDG 启动、任务报到与统一喂狗策略。
 * @addtogroup system
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_WATCHDOG_H
#define LAYERED_EMBEDDED_C_SYSTEM_WATCHDOG_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 参与看门狗报到的任务。
 */
typedef enum sys_watchdog_task_e {
  SYS_WATCHDOG_TASK_CONTROL = 0,
  SYS_WATCHDOG_TASK_UART,
  SYS_WATCHDOG_TASK_COUNT,
} sys_watchdog_task_t;

/**
 * @brief 启动 IWDG。
 * @param timeout_ms 目标超时（ms），内部夹紧到 1..PLATFORM_IWDG_MAX_TIMEOUT_MS。
 * @note IWDG 一旦启动不能停止；必须在监控任务准备好之后再调用。
 * @note 应在任务启动前调用，避免与 Sys_Watchdog_CheckIn 并发。
 * @note 实际超时受 LSI 频率偏差影响。
 */
void Sys_Watchdog_Init(uint32_t timeout_ms);

/**
 * @brief 任务报到。
 * @param task 任务标识。
 * @param now_ms 当前毫秒时间戳。
 * @note 只能在任务上下文调用，不能在 ISR 中调用。
 */
void Sys_Watchdog_CheckIn(sys_watchdog_task_t task, uint32_t now_ms);

/**
 * @brief 检查所有任务是否新鲜；全部新鲜时喂狗。
 * @param now_ms 当前毫秒时间戳。
 * @return true 已喂狗；false 存在超时任务，未喂狗。
 * @note 只能在任务上下文调用，不能在 ISR 中调用。
 * @note 由监控任务调用；不要在 ISR 或其他驱动里直接喂狗。
 */
bool Sys_Watchdog_Service(uint32_t now_ms);

/**
 * @brief 读取并清除 IWDG 复位标志。
 * @return true 本次复位由 IWDG 触发。
 */
bool Sys_Watchdog_WasReset(void);

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_WATCHDOG_H
