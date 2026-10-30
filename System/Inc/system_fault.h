//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file system_fault.h
 * @brief Fault 诊断与安全钩子。
 * @addtogroup system
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_FAULT_H
#define LAYERED_EMBEDDED_C_SYSTEM_FAULT_H

#include <stdint.h>

/**
 * @brief Fault 类型。
 */
typedef enum sys_fault_type_e {
  SYS_FAULT_HARD = 0,
  SYS_FAULT_MEMMANAGE,
  SYS_FAULT_BUS,
  SYS_FAULT_USAGE,
} sys_fault_type_t;

/**
 * @brief 最近一次 Fault 的快照。
 */
typedef struct sys_fault_info_s {
  sys_fault_type_t type;
  uint32_t       cfsr;
  uint32_t       hfsr;
  uint32_t       mmfar;
  uint32_t       bfar;
  uint32_t       msp;
  uint32_t       psp;
} sys_fault_info_t;

/**
 * @brief 捕获 Fault 信息并调用安全钩子。
 * @param type Fault 类型；写入快照的 type 字段。
 * @pre 只能在对应 Fault Handler 中调用，且中断上下文不得依赖阻塞或动态内存。
 * @post 快照已从 SCB 和核心寄存器读取，Sys_Fault_OnFault 已被调用，随后中断被全局屏蔽。
 * @warning 仅作调试兜底；钩子必须快速且不得返回正常控制流，调用方应在返回后停机。
 */
void Sys_Fault_Handler(sys_fault_type_t type);

/**
 * @brief 获取最近一次 Fault 快照。
 * @return 指向进程生命周期内静态 sys_fault_info_t 的非 NULL 只读指针。
 * @note 对象只在 Sys_Fault_Handler 中更新；未发生 Fault 时字段为静态零初始化。
 * @warning Fault 捕获期间读取可能看到部分更新的快照；应在停机后读取。
 */
const sys_fault_info_t *Sys_Fault_GetInfo(void);

/**
 * @brief 安全钩子，由 Board/App 提供强定义。
 * @fn void Sys_Fault_OnFault(const sys_fault_info_t *info)
 * @param info 指向 Sys_Fault_Handler 静态快照的只读指针；弱实现允许非 NULL。
 * @pre 只能在 Fault Handler 中调用；实现必须无阻塞、无动态内存，并尽快切换安全输出。
 * @post 默认弱实现不修改任何状态；强定义承担实际安全动作。
 * @warning 不得在钩子中返回依赖 RTOS 调度的路径，也不得等待其他任务或中断。
 */
void Sys_Fault_OnFault(const sys_fault_info_t *info);

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_FAULT_H
