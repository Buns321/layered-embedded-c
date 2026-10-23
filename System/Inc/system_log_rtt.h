//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file system_log_rtt.h
 * @brief System 日志后端的 SEGGER RTT 实现。
 * @addtogroup system
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_LOG_RTT_H
#define LAYERED_EMBEDDED_C_SYSTEM_LOG_RTT_H

#include "system_log_base.h"
#include <stdint.h>

/**
 * @brief RTT 日志后端对象。
 * @note 该对象必须是静态对象，或以其他方式保证生命周期长于 System 日志门面。
 */
typedef struct {
  sys_log_base_t base;
  uint32_t       channel;
} system_log_rtt_t;

/**
 * @brief 初始化 RTT 日志后端。
 * @param this 后端对象。
 * @param name 调试名称。
 * @param channel RTT 上行通道，通常为 0。
 */
void Sys_Log_RTT_Init(
  system_log_rtt_t *this,
  const char *name,
  uint32_t channel
);

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_LOG_RTT_H
