//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file system_fault.c
 * @brief System Fault 诊断与快照实现。
 * @ingroup system
 */

#include "system_fault.h"

#if defined(STM32F4xx)
#include "stm32f4xx.h"
#elif defined(STM32F1xx)
#include "stm32f1xx.h"
#else
#error "Unsupported STM32 family"
#endif

static sys_fault_info_t s_fault_info;

/**
 * @brief 安全钩子，由 Board/App 提供强定义。
 * @fn void Sys_Fault_OnFault(const sys_fault_info_t *info)
 * @param info 指向 Sys_Fault_Handler 静态快照的只读指针；弱实现允许非 NULL。
 * @pre 只能在 Fault Handler 中调用；实现必须无阻塞、无动态内存，并尽快切换安全输出。
 * @post 默认弱实现不修改任何状态；强定义承担实际安全动作。
 * @warning 不得在钩子中返回依赖 RTOS 调度的路径，也不得等待其他任务或中断。
 */
__attribute__((weak)) void Sys_Fault_OnFault(const sys_fault_info_t *info) {
  (void)info;
}

/**
 * @brief 捕获 Fault 信息并调用安全钩子。
 * @param type Fault 类型；写入快照的 type 字段。
 * @pre 只能在对应 Fault Handler 中调用，且中断上下文不得依赖阻塞或动态内存。
 * @post 快照已从 SCB 和核心寄存器读取，Sys_Fault_OnFault 已被调用，随后中断被全局屏蔽。
 * @warning 仅作调试兜底；钩子必须快速且不得返回正常控制流，调用方应在返回后停机。
 */
void Sys_Fault_Handler(sys_fault_type_t type) {
  s_fault_info.type = type;
  s_fault_info.cfsr = SCB->CFSR;
  s_fault_info.hfsr = SCB->HFSR;
  s_fault_info.mmfar = SCB->MMFAR;
  s_fault_info.bfar = SCB->BFAR;
  s_fault_info.msp = (uint32_t)__get_MSP();
  s_fault_info.psp = (uint32_t)__get_PSP();

  Sys_Fault_OnFault(&s_fault_info);

  __disable_irq();
}

/**
 * @brief 获取最近一次 Fault 快照。
 * @return 指向进程生命周期内静态 sys_fault_info_t 的非 NULL 只读指针。
 * @note 对象只在 Sys_Fault_Handler 中更新；未发生 Fault 时字段为静态零初始化。
 * @warning Fault 捕获期间读取可能看到部分更新的快照；应在停机后读取。
 */
const sys_fault_info_t *Sys_Fault_GetInfo(void) {
  return &s_fault_info;
}
