/**
 * @file sys_log_rtt.h
 * @brief System 日志 SEGGER RTT 非阻塞上行通道后端。
 * @addtogroup sys_core
 * @{
 */

#ifndef LAYERED_CAR_SYS_LOG_RTT_H
#define LAYERED_CAR_SYS_LOG_RTT_H

#include "sys_log_base.h"
#include <stdint.h>

/**
 * @brief RTT 日志后端对象（须静态分配）。
 */
typedef struct sys_log_rtt_s {
  sys_log_base_t base;
  uint32_t channel;
} sys_log_rtt_t;

/**
 * @brief 初始化 RTT 日志后端并设置对应上行通道为非阻塞跳过模式。
 */
void Sys_Log_RTT_Init(sys_log_rtt_t *log_h, const char *name, uint32_t channel);

/** @} */

#endif /* LAYERED_CAR_SYS_LOG_RTT_H */
