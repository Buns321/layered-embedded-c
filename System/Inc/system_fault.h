//
// Created by Buns_ on 2026/10/21.
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
typedef enum {
  SYS_FAULT_HARD = 0,
  SYS_FAULT_MEMMANAGE,
  SYS_FAULT_BUS,
  SYS_FAULT_USAGE,
} Sys_Fault_Type;

/**
 * @brief 最近一次 Fault 的快照。
 */
typedef struct {
  Sys_Fault_Type type;
  uint32_t       cfsr;
  uint32_t       hfsr;
  uint32_t       mmfar;
  uint32_t       bfar;
  uint32_t       msp;
  uint32_t       psp;
} Sys_Fault_Info;

/**
 * @brief 捕获 Fault 信息并调用安全钩子。
 * @param type Fault 类型。
 * @note 在 Fault Handler 中调用；返回后由调用方的 while(1) 兜底。
 */
void Sys_Fault_Handler(Sys_Fault_Type type);

/**
 * @brief 获取最近一次 Fault 快照。
 * @return 指向静态快照的指针；调试器可在停机后读取。
 */
const Sys_Fault_Info *Sys_Fault_GetInfo(void);

/**
 * @brief 安全钩子，由 Board/App 提供强定义。
 * @param info Fault 快照。
 * @note 默认弱实现为空；可在此切电机、记录或复位。
 */
void Sys_Fault_OnFault(const Sys_Fault_Info *info);

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_FAULT_H
