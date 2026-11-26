/**
 * @file sys_log_rtt.h
 * @brief System 日志 SEGGER RTT 非阻塞上行通道后端。
 * @details 属于 Layer 2（Framework/Core），将 sys_log_base_t 接口适配至 SEGGER RTT
 *          上行缓冲区（SEGGER_RTT_MODE_NO_BLOCK_SKIP），所有实例须静态分配。
 * @addtogroup Framework_Core
 * @{
 */

#ifndef LAYERED_CAR_SYS_LOG_RTT_H
#define LAYERED_CAR_SYS_LOG_RTT_H

#include "sys_log_base.h"
#include <stdint.h>

/**
 * @brief RTT 日志后端对象（须静态分配，首成员嵌入 sys_log_base_t）。
 */
typedef struct sys_log_rtt_s {
  sys_log_base_t base; /**< 日志后端抽象基类（必须位于结构体首成员）。 */
  uint32_t channel;    /**< SEGGER RTT 上行通道索引（0..SEGGER_RTT_MAX_NUM_UP_BUFFERS-1）。 */
} sys_log_rtt_t;

/**
 * @brief 初始化 RTT 日志后端并设置对应上行通道为非阻塞跳过模式。
 * @param[out] log_h   静态分配的 RTT 日志后端实例指针（不可为 NULL）。
 * @param[in]  name    后端名称字符串（不可为 NULL）。
 * @param[in]  channel SEGGER RTT 上行通道编号（须 < SEGGER_RTT_MAX_NUM_UP_BUFFERS）。
 * @post 对应 RTT 上行通道已配置为 SEGGER_RTT_MODE_NO_BLOCK_SKIP（缓冲满时直接丢弃）。
 */
void Sys_Log_RTT_Init(sys_log_rtt_t *log_h, const char *name, uint32_t channel);

/** @} */

#endif /* LAYERED_CAR_SYS_LOG_RTT_H */
