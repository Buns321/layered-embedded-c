/**
 * @file sys_fault.c
 * @brief Fault 异常现场捕获与安全钩子实现。
 * @ingroup sys_core
 */

#include "sys_fault.h"
#include "bsp_mcu.h"

static sys_fault_info_t s_fault_info;

__attribute__((weak)) void Sys_Fault_OnFault(const sys_fault_info_t *info) { (void)info; }

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

const sys_fault_info_t *Sys_Fault_GetInfo(void) { return &s_fault_info; }
