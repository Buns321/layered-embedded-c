/**
 * @file sys_critical.h
 * @brief 任务与 ISR 临界区保护原语。
 * @details 属于 Layer 2（Framework/Core）基础服务，统一封装 FreeRTOS 调度器启动前后的
 *          任务临界区与中断级临界区原语，确保跨任务/ISR 共享状态访问的原子性；零动态内存分配。
 *
 * @defgroup Framework_Core 核心基础设施
 * @{
 */

#ifndef LAYERED_CAR_SYS_CRITICAL_H
#define LAYERED_CAR_SYS_CRITICAL_H

#include <stdint.h>

/**
 * @brief ISR 临界区保存的中断屏蔽状态快照（对应 PRIMASK 或 BASEPRI 寄存器值）。
 */
typedef uint32_t sys_critical_state_t;

/**
 * @brief 进入任务或启动期临界区（支持嵌套计数）。
 * @pre 须与 Sys_Critical_Exit() 严格成对调用；严禁在 ISR 中调用。
 * @note 调度器未启动前通过保存 PRIMASK 并全局关中断实现嵌套；调度器启动后委托给
 * taskENTER_CRITICAL()。
 */
void Sys_Critical_Enter(void);

/**
 * @brief 退出任务或启动期临界区并按嵌套计数恢复中断状态。
 * @pre 必须先成对调用过 Sys_Critical_Enter()；严禁在 ISR 中调用。
 */
void Sys_Critical_Exit(void);

/**
 * @brief 进入 ISR 临界区并返回中断屏蔽状态快照。
 * @return 进入临界区前的中断屏蔽状态快照，须原样传回 Sys_Critical_ExitFromISR()。
 * @note 仅限中断服务程序（ISR）上下文调用。
 */
sys_critical_state_t Sys_Critical_EnterFromISR(void);

/**
 * @brief 退出 ISR 临界区并恢复先前保存的中断屏蔽状态。
 * @param[in] state 由同一次匹配的 Sys_Critical_EnterFromISR() 返回的中断屏蔽状态快照。
 * @note 仅限中断服务程序（ISR）上下文调用。
 */
void Sys_Critical_ExitFromISR(sys_critical_state_t state);

/** @} */

#endif /* LAYERED_CAR_SYS_CRITICAL_H */
