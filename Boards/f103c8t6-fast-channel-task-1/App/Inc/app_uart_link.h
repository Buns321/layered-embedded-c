//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file app_uart_link.h
 * @brief 板级 UART1 全双工链路：JustFloat TX 与参数命令 RX。
 * @addtogroup app
 * @{
 * @note 链路对象是物理 UART 的唯一回调所有者，并通过虚拟 uart_base_t 复用
 *       JustFloat 设备，不修改共享 UART 回调契约。
 */

#ifndef LAYERED_EMBEDDED_C_APP_UART_LINK_H
#define LAYERED_EMBEDDED_C_APP_UART_LINK_H

#include "app_command_parser.h"
#include "app_fast_channel_config.h"
#include "justfloat_uart.h"
#include "platform_def.h"
#include "uart_base.h"
#include "uart_port.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 参数命令处理回调。
 * @return true 表示命令已被接受。
 * @note 在遥测任务上下文调用。
 */
typedef bool (*app_uart_command_handler_t)(
  const app_uart_command_t *command,
  void *context
);

/**
 * @brief 链路命令结果。
 */
typedef enum app_uart_link_command_status_e {
  APP_UART_LINK_COMMAND_IDLE = 0U,
  APP_UART_LINK_COMMAND_ACCEPTED,
  APP_UART_LINK_COMMAND_REJECTED,
  APP_UART_LINK_COMMAND_FORMAT_ERROR,
  APP_UART_LINK_COMMAND_OVERFLOW,
} app_uart_link_command_status_t;

/**
 * @brief 链路运行状态。
 */
typedef struct app_uart_link_status_s {
  /** @brief 最近一次命令结果。 */
  app_uart_link_command_status_t last_command_status;
  /** @brief 初始化以来接受的命令数。 */
  uint32_t accepted_commands;
  /** @brief 初始化以来拒绝的命令数。 */
  uint32_t rejected_commands;
  /** @brief RX 环形缓冲满导致的丢字节次数。 */
  uint32_t rx_overflow_count;
  /** @brief UART 错误事件数。 */
  uint32_t uart_error_count;
  /** @brief JustFloat 后端状态。 */
  telemetry_status_t telemetry;
} app_uart_link_status_t;

/**
 * @brief 板级 UART1 链路对象。
 * @note telemetry_view 必须是首成员，供虚拟 uart ops 反查链路对象。
 * @note 所有缓冲和对象静态分配；ISR 只写 RX 环形缓冲，命令解析只在任务执行。
 */
typedef struct app_uart_link_s {
  uart_base_t telemetry_view;
  uart_port_t port;
  justfloat_uart_t telemetry;
  app_uart_command_handler_t command_handler;
  void *command_context;
  uint8_t rx_byte;
  uint8_t rx_ring[APP_UART_RX_RING_CAPACITY];
  char command_line[APP_UART_COMMAND_CAPACITY];
  volatile uint16_t rx_head;
  volatile uint16_t rx_tail;
  uint16_t command_length;
  volatile uint32_t rx_overflow_count;
  volatile uint32_t uart_error_count;
  volatile bool rx_rearm_pending;
  volatile bool initialized;
  app_uart_link_command_status_t last_command_status;
  uint32_t accepted_commands;
  uint32_t rejected_commands;
} app_uart_link_t;

/**
 * @brief 初始化 UART1 链路和 JustFloat。
 * @param link_h 对象。
 * @param name 名称。
 * @param command_handler 命令处理回调，不能为空。
 * @param command_context 回调上下文。
 * @retval PLATFORM_STATUS_OK 初始化和接收挂载成功。
 * @retval PLATFORM_STATUS_BUSY 物理 UART 回调已被占用。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 参数或 UART 实例非法。
 */
platform_status_t App_UART_LinkInit(
  app_uart_link_t *link_h,
  const char *name,
  app_uart_command_handler_t command_handler,
  void *command_context
);

/**
 * @brief 启动 USART1 单字节中断接收。
 * @retval PLATFORM_STATUS_OK 接收已启动。
 * @retval PLATFORM_STATUS_BUSY UART 正忙，稍后由命令处理阶段重试。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 对象未初始化。
 * @retval PLATFORM_STATUS_ERROR UART 启动失败。
 */
platform_status_t App_UART_LinkStart(app_uart_link_t *link_h);

/**
 * @brief 解析已接收参数命令。
 * @param link_h 链路对象。
 * @param now_ms 当前毫秒时间戳；当前保留，后续用于错误恢复节流。
 */
void App_UART_LinkProcessCommands(
  app_uart_link_t *link_h,
  uint32_t now_ms
);

/** @brief 非阻塞发送一帧 32 通道 JustFloat。 */
dev_telemetry_status_t App_UART_LinkSendTelemetry(
  app_uart_link_t *link_h,
  const telemetry_sample_t *sample
);

/**
 * @brief 复制链路状态。
 * @param link_h 链路对象。
 * @param status 输出状态。
 */
void App_UART_LinkGetStatus(
  app_uart_link_t *link_h,
  app_uart_link_status_t *status
);

/** @} */

#endif //LAYERED_EMBEDDED_C_APP_UART_LINK_H
