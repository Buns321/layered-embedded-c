/**
 * @file sys_fault.c
 * @brief Fault 异常现场捕获与安全钩子实现。
 * @details 属于 Layer 2（Framework/Core），经 bsp_mcu.h 访问 SCB 与核心栈指针寄存器，
 *          将故障现场锁存于静态全局结构体，供调试器回溯或安全钩子执行紧急停机。
 * @ingroup Framework_Core
 */

#include "sys_fault.h"
#include "bsp_mcu.h"

/** @brief 最近一次捕获的 Fault 现场寄存器静态快照。 */
static sys_fault_info_t s_fault_info;

/** @brief 默认弱实现的 Fault 安全钩子（空操作，供板级或应用层强覆盖；参数 info 为现场快照）。 */
__attribute__((weak)) void Sys_Fault_OnFault(const sys_fault_info_t *info) { (void)info; }

void Sys_Fault_Handler(sys_fault_type_t type) {
  // 先完整锁存 SCB 故障状态与 MSP/PSP，再触发安全钩子，防止钩子内操作改变现场寄存器
  s_fault_info.type = type;
  s_fault_info.cfsr = SCB->CFSR;
  s_fault_info.hfsr = SCB->HFSR;
  s_fault_info.mmfar = SCB->MMFAR;
  s_fault_info.bfar = SCB->BFAR;
  s_fault_info.msp = (uint32_t)__get_MSP();
  s_fault_info.psp = (uint32_t)__get_PSP();

  // 允许板级/应用层在系统锁死前切断执行器（如关闭电机 PWM 与 STBY）
  Sys_Fault_OnFault(&s_fault_info);

  // 屏蔽可屏蔽中断，确保系统安全停机后不再响应普通外设中断
  __disable_irq();
}

const sys_fault_info_t *Sys_Fault_GetInfo(void) { return &s_fault_info; }
