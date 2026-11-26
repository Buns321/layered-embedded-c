/**
 * @file sys_critical.h
 * @brief 任务与 ISR 临界区保护原语。
 * @addtogroup sys_core
 * @{
 */

#ifndef LAYERED_CAR_SYS_CRITICAL_H
#define LAYERED_CAR_SYS_CRITICAL_H

#include <stdint.h>

/**
 * @brief ISR 临界区保存的中断屏蔽状态快照。
 */
typedef uint32_t sys_critical_state_t;

/**
 * @brief 进入任务/启动期临界区（支持嵌套，须与 Exit 成对使用；严禁在 ISR 中调用）。
 */
void Sys_Critical_Enter(void);

/**
 * @brief 退出任务/启动期临界区。
 */
void Sys_Critical_Exit(void);

/**
 * @brief 进入 ISR 临界区并返回中断屏蔽快照。
 */
sys_critical_state_t Sys_Critical_EnterFromISR(void);

/**
 * @brief 退出 ISR 临界区并恢复状态快照。
 */
void Sys_Critical_ExitFromISR(sys_critical_state_t state);

/** @} */

#endif /* LAYERED_CAR_SYS_CRITICAL_H */
