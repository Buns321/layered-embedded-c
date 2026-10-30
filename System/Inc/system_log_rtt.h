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
typedef struct system_log_rtt_s {
  sys_log_base_t base;
  uint32_t       channel;
} system_log_rtt_t;

/**
 * @brief 初始化 RTT 日志后端。
 * @param log_h 待初始化的 RTT 后端对象；生命周期必须覆盖日志门面的全部使用期。
 * @param name 非空只读调试名称，生命周期必须覆盖日志门面的全部使用期。
 * @param channel RTT 上行通道号，范围 0..SEGGER_RTT_MAX_NUM_UP_BUFFERS-1，通常为 0。
 * @pre log_h 和 name 非 NULL，channel 在 SEGGER RTT 上行通道范围内。
 * @post base 已绑定 RTT 操作表和名称，channel 已保存，RTT 已初始化且该上行通道
 *       配置为 SEGGER_RTT_MODE_NO_BLOCK_SKIP。
 * @note 不分配动态内存；应在发布给其他任务或 ISR 前完成初始化。
 * @warning Debug 构建用 assert 检查参数；Release 构建不校验，非法 channel 会访问
 *          未定义的 RTT 缓冲区配置。
 */
void Sys_Log_RTT_Init(
  system_log_rtt_t *log_h,
  const char *name,
  uint32_t channel
);

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_LOG_RTT_H
