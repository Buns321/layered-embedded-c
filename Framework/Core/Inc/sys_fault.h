/**
 * @file sys_fault.h
 * @brief Cortex-M Fault 现场捕获与紧急安全钩子。
 * @details 属于 Layer 2（Framework/Core），用于在 HardFault / MemManage / BusFault / UsageFault
 *          异常中静态锁存 SCB 故障寄存器与栈指针现场，并触发安全停机钩子；零动态内存分配。
 * @addtogroup Framework_Core
 * @{
 */

#ifndef LAYERED_CAR_SYS_FAULT_H
#define LAYERED_CAR_SYS_FAULT_H

#include <stdint.h>

/**
 * @brief Cortex-M Fault 异常类型枚举。
 */
typedef enum sys_fault_type_e {
  SYS_FAULT_HARD = 0,  /**< 硬件错误（HardFault）。 */
  SYS_FAULT_MEMMANAGE, /**< 存储器管理错误（MemManage Fault）。 */
  SYS_FAULT_BUS,       /**< 总线错误（BusFault）。 */
  SYS_FAULT_USAGE,     /**< 用法错误（UsageFault）。 */
} sys_fault_type_t;

/**
 * @brief 最近一次 Fault 现场寄存器快照。
 */
typedef struct sys_fault_info_s {
  sys_fault_type_t type; /**< 触发本次快照的 Fault 异常类型。 */
  uint32_t cfsr;         /**< 可配置故障状态寄存器（SCB->CFSR）。 */
  uint32_t hfsr;         /**< 硬件故障状态寄存器（SCB->HFSR）。 */
  uint32_t mmfar;        /**< 存储器管理故障地址寄存器（SCB->MMFAR）。 */
  uint32_t bfar;         /**< 总线故障地址寄存器（SCB->BFAR）。 */
  uint32_t msp;          /**< 主栈指针快照（MSP）。 */
  uint32_t psp;          /**< 进程栈指针快照（PSP）。 */
} sys_fault_info_t;

/**
 * @brief 捕获 Fault 寄存器快照、调用 Sys_Fault_OnFault 并全局屏蔽中断。
 * @param[in] type 当前进入的 Fault 异常类型。
 * @warning 仅限 Fault 异常向量 ISR 调用；执行完毕后全局中断保持关闭状态。
 */
void Sys_Fault_Handler(sys_fault_type_t type);

/**
 * @brief 获取静态保存的最近一次 Fault 快照只读指针。
 * @return 指向静态分配的 sys_fault_info_t 结构体的只读指针（永不为 NULL）。
 */
const sys_fault_info_t *Sys_Fault_GetInfo(void);

/**
 * @brief Fault 安全钩子（弱定义可由板级或整车门面强覆盖）。
 * @param[in] info 指向已锁存的 Fault 现场寄存器快照的只读指针。
 * @note 运行于 Fault 异常上下文，禁止阻塞调用与动态内存分配，应仅执行紧急断电等安全动作。
 */
void Sys_Fault_OnFault(const sys_fault_info_t *info);

/** @} */

#endif /* LAYERED_CAR_SYS_FAULT_H */
