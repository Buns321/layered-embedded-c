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
 * @param command 已解析命令，只读；指针仅在本次回调期间有效。
 * @param context 初始化链路时传入的上下文，可为 NULL，由回调实现解释。
 * @retval true 命令已接受并完成对应状态变更。
 * @retval false 命令名未知、数值越界或当前状态不允许该变更。
 * @note 在 VofaTask 上下文同步调用；回调不得阻塞，也不得保留 command 指针。
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
 * @brief 板级 UART1 链路实例。
 * @note telemetry_view 必须是首成员，供虚拟 uart ops 反查链路实例。
 * @note 所有缓冲和对象静态分配；ISR 只生产 RX 字节并更新接收计数/重挂标志，命令解析只在任务执行。
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
 * @param link_h 待初始化的静态链路对象；函数会先清零全部字段。
 * @param name 链路对象名，NUL 结尾，供 UART/JustFloat 诊断使用；必须非 NULL，空字符串会按名称接受。
 * @param command_handler 参数命令处理回调，必须非 NULL；链路不复制其代码或上下文。
 * @param command_context 传给 command_handler 的上下文，可为 NULL。
 * @retval PLATFORM_STATUS_OK 虚拟 UART、物理 UART、回调和 JustFloat 已装配。
 * @retval PLATFORM_STATUS_BUSY 物理 UART 实例的回调槽已被其他所有者占用。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 必需指针为空、UART 实例非法或配置不满足底层要求。
 * @retval PLATFORM_STATUS_ERROR 物理 UART 端口初始化失败。
 * @pre 已初始化 USART1 外设；调度器尚未启动或不希望并发访问该链路。
 * @pre link_h、name 和回调上下文的生命周期覆盖链路完整使用期；link_h 由函数清零并接管，通常使用静态存储。
 * @post 成功时链路拥有物理 UART 的唯一回调槽，但 RX 中断尚未启动，必须再调用 App_UART_LinkStart。
 * @note 不分配动态内存；失败后链路不可继续使用，调用方应停止初始化流程。
 */
platform_status_t App_UART_LinkInit(
  app_uart_link_t *link_h,
  const char *name,
  app_uart_command_handler_t command_handler,
  void *command_context
);

/**
 * @brief 启动 USART1 单字节中断接收。
 * @param link_h 已由 App_UART_LinkInit 成功初始化的链路实例；非 NULL 且 initialized 为 true。
 * @retval PLATFORM_STATUS_OK 单字节 IT 接收已挂载，rx_rearm_pending 清零。
 * @retval PLATFORM_STATUS_BUSY 物理 UART 忙；已在链路中标记待重挂，由命令处理阶段重试。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT link_h 为 NULL 或尚未完成初始化。
 * @retval PLATFORM_STATUS_ERROR 物理 UART 返回非忙错误或实例状态非法。
 * @pre 只允许在链路初始化完成后、任务开始处理命令前调用。
 * @post 返回 BUSY 或 ERROR 时链路仍保留重挂意图，不会静默丢弃接收恢复。
 * @note 不分配内存；在任务上下文调用，底层 IT 启动不得在 RX ISR 内重入。
 */
platform_status_t App_UART_LinkStart(app_uart_link_t *link_h);

/**
 * @brief 解析已接收参数命令。
 * @param link_h 链路对象；NULL 或未初始化时函数不执行任何操作。
 * @param now_ms 当前单调毫秒时间戳；当前未参与解析或节流，仅保留为任务时间戳接口。
 * @pre 只允许 VofaTask 调用；RX ISR 只生产字节，不能与消费者同时解析同一命令行。
 * @post 已消费环形缓冲中的全部字节；完整 `\n` 行会同步调用命令回调，`\r` 被忽略。
 * @post 缓冲满时清空已收集内容，对每个被丢弃字节递增 rejected_commands 并置 OVERFLOW；后续字节从行首重新收集。
 * @post 重挂失败会在下次调用重试；后续完整行可能把 last_command_status 覆盖为 ACCEPTED 或 REJECTED。
 * @note 不分配内存；回调在调用者上下文执行，环形缓冲采用单生产者 ISR、单消费者任务模型。
 */
void App_UART_LinkProcessCommands(
  app_uart_link_t *link_h,
  uint32_t now_ms
);

/**
 * @brief 非阻塞发送一帧 32 通道 JustFloat。
 * @param link_h 已初始化的链路对象，必须非 NULL。
 * @param sample 待发送的一致样本；只读，调用方保留所有权，函数返回后即可释放或复用。
 * @retval DEV_TELEMETRY_OK 整帧已复制到 JustFloat 发送缓冲并由 DMA 接管。
 * @retval DEV_TELEMETRY_BUSY 上一帧仍在发送；本帧整帧丢弃，不等待。
 * @retval DEV_TELEMETRY_ERROR 样本、通道掩码或 UART 后端状态非法，整帧未发送。
 * @pre link_h 与 sample 非 NULL，且链路已完成初始化和接收启动。
 * @post 本函数不阻塞；发送缓冲和 DMA 数据的所有权在完成回调前归 JustFloat 发送器所有。
 * @note 只允许 VofaTask 调用；调试构建对空指针 assert，不分配内存。
 */
dev_telemetry_status_t App_UART_LinkSendTelemetry(
  app_uart_link_t *link_h,
  const telemetry_sample_t *sample
);

/**
 * @brief 复制链路状态。
 * @param link_h 链路对象，必须非 NULL；计数器可能正被 RX/TX ISR 更新。
 * @param status 输出快照；函数完整覆盖其字段，所有权仍归调用方。
 * @pre link_h 和 status 非 NULL；status 可写且不与链路对象重叠。
 * @post last_command_status、命令计数、RX 溢出/错误计数和 JustFloat 状态均已复制。
 * @note 各计数为独立读取，不保证跨字段的全局原子快照；不分配内存，不访问硬件寄存器。
 */
void App_UART_LinkGetStatus(
  app_uart_link_t *link_h,
  app_uart_link_status_t *status
);

/** @} */

#endif //LAYERED_EMBEDDED_C_APP_UART_LINK_H
