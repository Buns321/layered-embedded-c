//
// Created by Buns_ on 2026/10/21.
//

/**
 * @file system_critical.h
 * @brief 临界区与中断状态保存/恢复。
 * @addtogroup system
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_CRITICAL_H
#define LAYERED_EMBEDDED_C_SYSTEM_CRITICAL_H

#include <stdint.h>

/**
 * @brief ISR 临界区返回的保存状态。
 */
typedef uint32_t Sys_Critical_State;

/**
 * @brief 进入任务上下文临界区。
 * @note 必须与 Sys_Critical_Exit 成对使用。
 * @warning 只包住几条指令；不要放阻塞调用、串口、浮点三角函数或策略解算。
 */
void Sys_Critical_Enter(void);

/**
 * @brief 退出任务上下文临界区。
 */
void Sys_Critical_Exit(void);

/**
 * @brief 进入 ISR 上下文临界区。
 * @return 进入前的中断状态，供退出时恢复。
 * @note 必须与 Sys_Critical_ExitFromISR 成对使用。
 */
Sys_Critical_State Sys_Critical_EnterFromISR(void);

/**
 * @brief 退出 ISR 上下文临界区并恢复中断状态。
 * @param state Sys_Critical_EnterFromISR 返回的状态。
 */
void Sys_Critical_ExitFromISR(Sys_Critical_State state);

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_CRITICAL_H
