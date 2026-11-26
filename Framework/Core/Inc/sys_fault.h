/**
 * @file sys_fault.h
 * @brief Cortex-M Fault 现场捕获与紧急安全钩子。
 * @addtogroup sys_core
 * @{
 */

#ifndef LAYERED_CAR_SYS_FAULT_H
#define LAYERED_CAR_SYS_FAULT_H

#include <stdint.h>

/**
 * @brief Fault 异常类型。
 */
typedef enum sys_fault_type_e {
  SYS_FAULT_HARD = 0,
  SYS_FAULT_MEMMANAGE,
  SYS_FAULT_BUS,
  SYS_FAULT_USAGE,
} sys_fault_type_t;

/**
 * @brief 最近一次 Fault 现场寄存器快照。
 */
typedef struct sys_fault_info_s {
  sys_fault_type_t type;
  uint32_t cfsr;
  uint32_t hfsr;
  uint32_t mmfar;
  uint32_t bfar;
  uint32_t msp;
  uint32_t psp;
} sys_fault_info_t;

/**
 * @brief 捕获 Fault 寄存器快照、调用 Sys_Fault_OnFault 并全局屏蔽中断（仅 Fault ISR 调用）。
 */
void Sys_Fault_Handler(sys_fault_type_t type);

/**
 * @brief 获取静态保存的最近一次 Fault 快照只读指针。
 */
const sys_fault_info_t *Sys_Fault_GetInfo(void);

/**
 * @brief Fault 安全钩子（弱定义可由板级/门面强覆盖，禁止阻塞与动态分配）。
 */
void Sys_Fault_OnFault(const sys_fault_info_t *info);

/** @} */

#endif /* LAYERED_CAR_SYS_FAULT_H */
