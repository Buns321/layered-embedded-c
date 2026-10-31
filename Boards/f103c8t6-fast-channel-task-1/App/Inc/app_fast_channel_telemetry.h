/**
 * @file app_fast_channel_telemetry.h
 * @brief 快速通道任务一 32 通道遥测映射。
 * @addtogroup app
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_APP_FAST_CHANNEL_TELEMETRY_H
#define LAYERED_EMBEDDED_C_APP_FAST_CHANNEL_TELEMETRY_H

#include "app_fast_channel_config.h"
#include "telemetry_base.h"
#include "uart_link.h"
#include <stdint.h>

typedef struct app_fast_channel_snapshot_s app_fast_channel_snapshot_t;

/**
 * @brief App 参数命令最近结果。
 */
typedef enum app_fast_channel_command_status_e {
  APP_FAST_CHANNEL_COMMAND_IDLE = 0U,
  APP_FAST_CHANNEL_COMMAND_ACCEPTED,
  APP_FAST_CHANNEL_COMMAND_REJECTED,
  APP_FAST_CHANNEL_COMMAND_FORMAT_ERROR,
  APP_FAST_CHANNEL_COMMAND_OVERFLOW,
} app_fast_channel_command_status_t;

/**
 * @brief 遥测模块内部节奏状态。
 */
typedef struct app_fast_channel_telemetry_s {
  uint32_t last_log_ms;
  uint32_t observed_line_overflow_count;
} app_fast_channel_telemetry_t;

/**
 * @brief 初始化遥测模块状态。
 * @param telemetry_h 待初始化模块状态，不得为 NULL。
 * @post 日志节流和链路溢出观察计数清零。
 * @note 不访问硬件、不分配内存。
 */
void App_FastChannelTelemetry_Init(
  app_fast_channel_telemetry_t *telemetry_h
);

/**
 * @brief 组装并发送一帧 32 通道 JustFloat，同时输出低频状态日志。
 * @param telemetry_h 已初始化模块状态，不得为 NULL。
 * @param telemetry_base_h 已初始化遥测发送后端，不得为 NULL。
 * @param snapshot 当前 IMU 会话快照，不得为 NULL。
 * @param link_status 当前 UART 链路状态，不得为 NULL。
 * @param command_status 最近一次 App 命令结果。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @pre 只由 VofaTask 调用。
 * @post 已发起一次非阻塞整帧发送；忙时由后端整帧丢弃。
 * @note 不阻塞等待 DMA、不分配内存。
 */
void App_FastChannelTelemetry_Process(
  app_fast_channel_telemetry_t *telemetry_h,
  telemetry_base_t *telemetry_base_h,
  const app_fast_channel_snapshot_t *snapshot,
  const uart_link_status_t *link_status,
  app_fast_channel_command_status_t command_status,
  uint32_t now_ms
);

/** @} */

#endif //LAYERED_EMBEDDED_C_APP_FAST_CHANNEL_TELEMETRY_H
