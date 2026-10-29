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
 * @fn void Sys_Fault_OnFault(const sys_fault_info_t *info)
 * @brief 安全钩子，由 Board/App 提供强定义。
 * @param info Fault 快照。
 * @note 默认弱实现为空；可在此切电机、记录或复位。
 */
__attribute__((weak)) void Sys_Fault_OnFault(const sys_fault_info_t *info) {
  (void)info;
}

/**
 * @brief 捕获 Fault 信息并调用安全钩子。
 * @param type Fault 类型。
 * @note 在 Fault Handler 中调用；返回后由调用方的 while(1) 兜底。
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
 * @return 指向静态快照的指针；调试器可在停机后读取。
 */
const sys_fault_info_t *Sys_Fault_GetInfo(void) {
  return &s_fault_info;
}
