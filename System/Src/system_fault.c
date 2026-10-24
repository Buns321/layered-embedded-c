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

__attribute__((weak)) void Sys_Fault_OnFault(const sys_fault_info_t *info) {
  (void)info;
}

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

const sys_fault_info_t *Sys_Fault_GetInfo(void) {
  return &s_fault_info;
}
