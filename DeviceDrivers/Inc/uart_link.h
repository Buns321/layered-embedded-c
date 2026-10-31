/**
 * @file uart_link.h
 * @brief 基于 UART 的单生产/单消费文本行链路。
 * @addtogroup drivers_concrete
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_UART_LINK_H
#define LAYERED_EMBEDDED_C_UART_LINK_H

#include "platform_uart.h"
#include "system_byte_ring.h"
#include "uart_base.h"
#include "uart_port.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct uart_link_s uart_link_t;

/**
 * @brief 完整文本行处理回调。
 * @param line 可写且以 NUL 结尾的完整文本行；不含换行符，仅在本次回调期间有效。
 * @param now_ms 当前单调毫秒时间戳，单位 ms；同一次 Process 调用中的多行共享该值。
 * @param context 初始化时注册的回调上下文，可为 NULL。
 * @retval true 行已被接受。
 * @retval false 行被拒绝。
 * @pre line 和 context 的生命周期分别覆盖本次调用和链路生命周期。
 * @post 回调不得保存 line 指针，也不得在返回后继续访问其内容。
 * @note 在任务上下文执行；不得阻塞、分配内存或执行长耗时解析。
 */
typedef bool (*uart_link_line_handler_t)(
  char *line,
  uint32_t now_ms,
  void *context
);

/**
 * @brief UART 文本行链路配置。
 * @note 所有缓冲区由调用方静态提供，链路不取得所有权且不复制配置。
 */
typedef struct uart_link_config_s {
  /** @brief 调试名称，必须在链路生命周期内有效。 */
  const char *name;
  /** @brief 已由 CubeMX 初始化的 Platform UART 实例。 */
  platform_uart_instance_t instance;
  /** @brief RX 环形缓冲存储，容量至少为 2 字节且必须为 2 的幂。 */
  uint8_t *rx_buffer;
  /** @brief RX 环形缓冲容量，单位 byte；须为 2 的幂，实际可存容量为 capacity - 1。 */
  uint16_t rx_capacity_bytes;
  /** @brief 文本行缓冲存储，容量至少为 2 字节。 */
  char *line_buffer;
  /** @brief 文本行缓冲容量，单位 byte，包含结尾 NUL。 */
  uint16_t line_capacity_bytes;
  /** @brief 完整文本行处理回调，不得为 NULL。 */
  uart_link_line_handler_t line_handler;
  /** @brief 传给 line_handler 的上下文，可为 NULL。 */
  void *line_context;
} uart_link_config_t;

/**
 * @brief UART 文本行链路运行状态。
 */
typedef struct uart_link_status_s {
  /** @brief 初始化已完成且开始过接收。 */
  bool is_started;
  /** @brief 接收尚未成功挂接，任务处理时会继续重试。 */
  bool receive_pending;
  /** @brief 接收完成中断收到的字节总数。 */
  uint32_t rx_bytes;
  /** @brief 因 RX 环形缓冲满而丢弃的字节数。 */
  uint32_t rx_dropped_bytes;
  /** @brief 因文本行超长而丢弃的行数。 */
  uint32_t line_overflow_count;
  /** @brief UART 错误事件数。 */
  uint32_t uart_error_count;
  /** @brief 重挂接收失败次数，包含初始启动失败。 */
  uint32_t rearm_error_count;
  /** @brief 已组出的完整文本行数。 */
  uint32_t complete_lines;
  /** @brief line_handler 接受的行数。 */
  uint32_t accepted_lines;
  /** @brief line_handler 拒绝的行数。 */
  uint32_t rejected_lines;
} uart_link_status_t;

/**
 * @brief UART 文本行链路对象。
 * @note uart_view 必须是首成员，供虚拟 UART ops 反查链路对象。
 * @note ISR 只生产 RX 环形缓冲并尝试重挂；任务只消费环形缓冲和组帧。
 */
typedef struct uart_link_s {
  uart_base_t uart_view;
  uart_port_t port;
  const char *name;
  uint8_t *rx_buffer;
  uint16_t rx_capacity_bytes;
  char *line_buffer;
  uint16_t line_capacity_bytes;
  uart_link_line_handler_t line_handler;
  void *line_context;

  uint8_t rx_byte;
  sys_byte_ring_t rx_ring;
  uint16_t line_length_bytes;
  bool line_overflow;
  volatile bool is_started;
  volatile bool receive_pending;

  volatile uint32_t rx_bytes;
  volatile uint32_t line_overflow_count;
  volatile uint32_t uart_error_count;
  volatile uint32_t rearm_error_count;
  volatile uint32_t complete_lines;
  volatile uint32_t accepted_lines;
  volatile uint32_t rejected_lines;
} uart_link_t;

/**
 * @brief 初始化 UART 文本行链路并取得物理 UART 的唯一回调所有权。
 * @param link_h 待初始化的静态链路对象，不得为 NULL；函数会覆盖其全部字段。
 * @param config 非空配置；RX 容量须为不小于 2 的 2 的幂，文本行容量须至少为 2，回调不得为 NULL。
 * @retval PLATFORM_STATUS_OK 虚拟 UART、物理端口和回调已成功装配。
 * @retval PLATFORM_STATUS_BUSY 物理 UART 已注册其他回调所有者。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 配置字段、缓冲容量或 UART 实例非法。
 * @retval PLATFORM_STATUS_ERROR 物理 UART 端口初始化失败。
 * @pre 硬件 UART 已由 CubeMX 完成初始化；链路尚未被并发使用。
 * @post 成功时链路拥有物理 UART 回调槽，但尚未启动 RX；必须再调用 Start。
 * @post 失败时对象内容不作可用性保证，调用方不得继续使用该链路。
 * @note 不分配内存；仅可在初始化阶段单线程调用。
 */
platform_status_t Dev_UARTLink_Init(
  uart_link_t *link_h,
  const uart_link_config_t *config
);

/**
 * @brief 启动单字节中断接收。
 * @param link_h 已成功初始化的链路对象，不得为 NULL。
 * @retval PLATFORM_STATUS_OK 接收已成功挂接。
 * @retval PLATFORM_STATUS_BUSY UART 暂时忙；链路已记录待重挂状态。
 * @retval PLATFORM_STATUS_TIMEOUT 后端以超时拒绝启动；链路已记录待重挂状态。
 * @retval PLATFORM_STATUS_ERROR 参数或硬件状态非法；链路已记录待重挂状态。
 * @pre link_h 已完成 Dev_UARTLink_Init，且当前没有并发调用 Start。
 * @post 无论返回值为何，后续 Process 都会在 receive_pending 为 true 时继续尝试重挂。
 * @note 不阻塞等待 UART 空闲，不分配内存；通常由任务初始化阶段调用一次。
 */
platform_status_t Dev_UARTLink_Start(uart_link_t *link_h);

/**
 * @brief 重试接收挂接并处理全部已入队文本行。
 * @param link_h 已初始化的链路对象，不得为 NULL。
 * @param now_ms 当前单调毫秒时间戳，单位 ms；传给本次组出的全部完整行。
 * @pre 只允许单一任务消费者调用，且不得与链路重新 Init 并发。
 * @post receive_pending 为 true 时尝试重挂接收；RX 环形缓冲被消费到追平写指针。
 * @post 每个由 LF 结束的行调用一次 line_handler；该行之前的 CR 被忽略。
 * @post 超长行丢弃到下一个 LF，并递增 line_overflow_count。
 * @note 不阻塞、不分配内存；回调同步执行，不得在 ISR 调用本函数。
 */
void Dev_UARTLink_Process(uart_link_t *link_h, uint32_t now_ms);

/**
 * @brief 取得虚拟 UART 视图，供 JustFloat 等 TX 后端使用。
 * @param link_h 已初始化的链路对象，可为 NULL。
 * @return link_h 非空时返回其内嵌 uart_base_t 指针，否则返回 NULL。
 * @pre 返回指针仅在 link_h 生命周期内有效。
 * @post 不修改链路状态，不取得额外所有权。
 * @note 虚拟 UART 的 TX 完成事件由物理 UART 回调转发；RX 文本行由本模块消费。
 */
uart_base_t *Dev_UARTLink_GetUartView(uart_link_t *link_h);

/**
 * @brief 复制链路状态计数。
 * @param link_h 已初始化的链路对象，不得为 NULL。
 * @param status 调用方拥有的输出快照，不得为 NULL；函数覆盖其全部字段。
 * @pre status 可写且不与 link_h 重叠。
 * @post 输出包含启动、重挂、收发和行处理计数。
 * @note 各 volatile 字段独立读取，不保证跨字段来自同一精确时刻；不分配内存。
 */
void Dev_UARTLink_GetStatus(
  const uart_link_t *link_h,
  uart_link_status_t *status
);

/** @} */

#endif //LAYERED_EMBEDDED_C_UART_LINK_H
