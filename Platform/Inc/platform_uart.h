//
// 由 Buns_ 创建于 2026/8/24。
//

/**
 * @file platform_uart.h
 * @brief UART 抽象：收发、IT/DMA、回调注册与错误清理。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_UART_H
#define LAYERED_EMBEDDED_C_PLATFORM_UART_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

/**
 * @brief UART 实例身份。
 * @note 未启用的实例不会进入该枚举。
 */
typedef enum platform_uart_instance_e {
#if PLATFORM_ENABLE_UART1
  PLATFORM_UART_1 = 0U,
#endif
#if PLATFORM_ENABLE_UART2
  PLATFORM_UART_2 = 1U,
#endif
#if PLATFORM_ENABLE_UART3
  PLATFORM_UART_3 = 2U,
#endif
#if PLATFORM_ENABLE_UART4
  PLATFORM_UART_4 = 3U,
#endif
#if PLATFORM_ENABLE_UART5
  PLATFORM_UART_5 = 4U,
#endif
#if PLATFORM_ENABLE_UART6
  PLATFORM_UART_6 = 5U,
#endif
} platform_uart_instance_t;

#if !PLATFORM_ENABLE_UART1 && !PLATFORM_ENABLE_UART2 && \
    !PLATFORM_ENABLE_UART3 && !PLATFORM_ENABLE_UART4 && \
    !PLATFORM_ENABLE_UART5 && !PLATFORM_ENABLE_UART6
#error "At least one Platform UART instance must be enabled"
#endif

/**
 * @brief UART 错误位。
 * @note 多个位可以同时置位。
 */
typedef enum platform_uart_error_e {
  PLATFORM_UART_ERROR_NONE    = 0u,
  PLATFORM_UART_ERROR_PARITY  = (1u << 0),
  PLATFORM_UART_ERROR_NOISE   = (1u << 1),
  PLATFORM_UART_ERROR_FRAME   = (1u << 2),
  PLATFORM_UART_ERROR_OVERRUN = (1u << 3),
  PLATFORM_UART_ERROR_DMA     = (1u << 5),
  PLATFORM_UART_ERROR_UNKNOWN = (1u << 7),
} platform_uart_error_t;

/** @brief UART 异步事件。 */
typedef enum platform_uart_event_type_e {
  PLATFORM_UART_EVENT_TX_COMPLETE = 0U,
  PLATFORM_UART_EVENT_TX_HALF_COMPLETE,
  PLATFORM_UART_EVENT_RX_COMPLETE,
  PLATFORM_UART_EVENT_RX_HALF_COMPLETE,
  PLATFORM_UART_EVENT_ERROR,
  PLATFORM_UART_EVENT_ABORT_COMPLETE,
  PLATFORM_UART_EVENT_ABORT_TX_COMPLETE,
  PLATFORM_UART_EVENT_ABORT_RX_COMPLETE,
} platform_uart_event_type_t;

/** @brief UART 事件数据，只在回调期间有效。 */
typedef struct platform_uart_event_s {
  platform_uart_event_type_t type;
  uint32_t error;
} platform_uart_event_t;

typedef void (*platform_uart_callback_t)(
  platform_uart_instance_t uart,
  const platform_uart_event_t *event,
  void *context
);

/**
 * @brief Platform UART 回调集合。
 * @note 回调在中断上下文执行，回调内不要做阻塞操作。
 * @note 注册时结构体内容会被复制。
 * @note 一个物理 UART 只有一个回调所有者。
 */
typedef struct platform_uart_callback_binding_s {
  platform_uart_callback_t callback;
  void *context;
} platform_uart_callback_binding_t;

/**
 * @brief 阻塞接收。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param pdata 接收缓冲区；不得为 NULL，调用期间可写且容量至少为 size 字节。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @param timeout 超时时间，单位 ms；PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 已接收 size 字节。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、缓冲区或长度非法；不访问 UART。
 * @retval PLATFORM_STATUS_BUSY 实例正忙。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内未收满 size 字节。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件报告接收失败。
 * @pre 对应 UART 的 MX_USARTx_UART_Init() 已完成。
 * @post 成功时 pdata 前 size 字节已填充；失败时内容可能部分更新，不得假定完整。
 * @warning 只能在任务上下文调用；同一实例的事务必须串行化，pdata 必须覆盖整个调用。
 * @note 不分配内存、不自动重新挂起接收；失败时记录对应 PLATFORM_ERROR_* 位。
 */
platform_status_t Platform_UART_Receive(
  platform_uart_instance_t uart,
  uint8_t *pdata,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief 阻塞发送。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param pdata 发送缓冲区；不得为 NULL，调用期间只读且至少包含 size 字节。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @param timeout 超时时间，单位 ms；PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 已发送 size 字节。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、缓冲区或长度非法；不访问 UART。
 * @retval PLATFORM_STATUS_BUSY 实例正忙。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内未完成发送。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件报告发送失败。
 * @pre 对应 UART 的 MX_USARTx_UART_Init() 已完成。
 * @post 成功时 size 字节已发送；本层不复制或释放 pdata。
 * @warning 只能在任务上下文调用；同一实例的事务必须串行化，pdata 必须覆盖整个调用。
 * @note 不分配内存；失败时记录对应 PLATFORM_ERROR_* 位。
 */
platform_status_t Platform_UART_Transmit(
  platform_uart_instance_t uart,
  const uint8_t *pdata,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief 中断接收。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param pdata 接收缓冲区；不得为 NULL，容量至少为 size 字节。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受中断接收请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、缓冲区或长度非法；不访问 UART。
 * @retval PLATFORM_STATUS_BUSY 实例已有活动接收或 HAL 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件无法启动请求。
 * @pre MX_USARTx_UART_Init() 和 NVIC 已完成配置；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受；完成、错误或中止通过回调异步通知。
 * @warning pdata 必须在 RX 完成、错误或中止前保持有效且不得并发访问；回调在中断上下文执行。
 * @note Platform 不复制缓冲区，也不自动重新挂起接收；重新挂接收由设备驱动负责。
 */
platform_status_t Platform_UART_ReceiveIT(
  platform_uart_instance_t uart,
  uint8_t *pdata,
  uint16_t size
);

/**
 * @brief 中断发送。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param pdata 发送缓冲区；不得为 NULL，至少包含 size 字节。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受中断发送请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、缓冲区或长度非法；不访问 UART。
 * @retval PLATFORM_STATUS_BUSY 实例已有活动发送或 HAL 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件无法启动请求。
 * @pre MX_USARTx_UART_Init() 和 NVIC 已完成配置；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受；完成、错误或中止通过回调异步通知。
 * @warning pdata 必须在 TX 完成、错误或中止前保持有效且内容不变；回调在中断上下文执行。
 * @note Platform 不复制缓冲区，也不自动重新启动发送。
 */
platform_status_t Platform_UART_TransmitIT(
  platform_uart_instance_t uart,
  const uint8_t *pdata,
  uint16_t size
);

/**
 * @brief DMA 接收。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param pdata 接收缓冲区；不得为 NULL，容量至少为 size 字节。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受 DMA 接收请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、缓冲区或长度非法；不访问 UART。
 * @retval PLATFORM_STATUS_BUSY 实例已有活动接收或 DMA 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或 DMA 无法启动请求。
 * @pre MX_USARTx_UART_Init() 和 CubeMX RX DMA 配置已完成；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受；完成、错误或中止通过回调异步通知。
 * @warning pdata 必须在 RX 完成、错误或中止前保持有效且不得并发访问；半完成时只能访问已完成的那一半。
 * @note Platform 不复制缓冲区，也不自动重新挂起接收；重新挂接收由设备驱动负责。
 */
platform_status_t Platform_UART_ReceiveDMA(
  platform_uart_instance_t uart,
  uint8_t *pdata,
  uint16_t size
);

/**
 * @brief DMA 发送。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param pdata 发送缓冲区；不得为 NULL，至少包含 size 字节。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受 DMA 发送请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、缓冲区或长度非法；不访问 UART。
 * @retval PLATFORM_STATUS_BUSY 实例已有活动发送或 DMA 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或 DMA 无法启动请求。
 * @pre MX_USARTx_UART_Init() 和 CubeMX TX DMA 配置已完成；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受；完成、错误或中止通过回调异步通知。
 * @warning pdata 必须在 TX 完成、错误或中止前保持有效且内容不变；回调在中断上下文执行。
 * @note Platform 不复制缓冲区，也不自动重新启动发送。
 */
platform_status_t Platform_UART_TransmitDMA(
  platform_uart_instance_t uart,
  const uint8_t *pdata,
  uint16_t size
);

/**
 * @brief 中止接收，清除 UART 硬件错误标志和 HAL 错误码。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @retval PLATFORM_STATUS_OK 接收中止完成，RX 标志和 HAL 错误码已清除。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界或未启用；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告实例忙。
 * @retval PLATFORM_STATUS_ERROR HAL 中止失败；不得假定接收已经停止。
 * @pre 调用方已启动接收或确认实例可接受中止请求，且同一实例没有并发的中止操作。
 * @post 成功时可显式重新启动接收；失败时保留 HAL 报告的硬件状态。
 * @warning 只能在任务上下文调用；内部可能同步等待 HAL 完成中止。
 * @note 调用开始时清除软件错误快照；不释放接收缓冲区。
 */
platform_status_t Platform_UART_AbortReceive(
  platform_uart_instance_t uart
);

/**
 * @brief 中止发送并清除 HAL 错误码。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @retval PLATFORM_STATUS_OK 发送中止完成，HAL 错误码已清除。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界或未启用；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告实例忙。
 * @retval PLATFORM_STATUS_ERROR HAL 中止失败；不得假定发送已经停止。
 * @pre 调用方已启动发送或确认实例可接受中止请求，且同一实例没有并发的中止操作。
 * @post 成功时可显式重新启动发送；失败时保留 HAL 报告的硬件状态。
 * @warning 只能在任务上下文调用；内部可能同步等待 HAL 完成中止。
 * @note 调用开始时清除软件错误快照；不释放发送缓冲区。
 */
platform_status_t Platform_UART_AbortTransmit(
  platform_uart_instance_t uart
);

/**
 * @brief 中止发送和接收，并清除 UART 错误状态。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @retval PLATFORM_STATUS_OK 双向传输已中止，RX 标志和 HAL 错误码已清除。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界或未启用；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告实例忙。
 * @retval PLATFORM_STATUS_ERROR HAL 中止失败；不得假定发送和接收都已停止。
 * @pre 调用方已启动需要中止的传输，且同一实例没有并发的中止或回调状态修改。
 * @post 成功时发送和接收均可显式重新启动；失败时保留 HAL 报告的硬件状态。
 * @warning 只能在任务上下文调用；内部可能同步等待 HAL 完成中止。
 * @note 调用开始时清除软件错误快照；不释放任何缓冲区。
 */
platform_status_t Platform_UART_Abort(
  platform_uart_instance_t uart
);

/**
 * @brief 兼容旧接口：中止接收并清除错误状态。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @retval PLATFORM_STATUS_OK 接收中止完成，RX 标志和 HAL 错误码已清除。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界或未启用；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告实例忙。
 * @retval PLATFORM_STATUS_ERROR HAL 中止失败；不得假定接收已经停止。
 * @pre 调用方已启动接收或确认实例可接受中止请求，且同一实例没有并发的中止操作。
 * @post 直接转交接收中止操作，不执行额外的缓冲区清理；新代码不得依赖该别名。
 * @warning 该接口已弃用，新代码必须使用 Platform_UART_AbortReceive()；只能在任务上下文
 *          调用，并遵守该接口的并发限制。
 * @note 保留该接口仅为兼容旧调用，不分配内存。
 */
platform_status_t Platform_UART_ClearBuffer(
  platform_uart_instance_t uart
);

/**
 * @brief 读取最近一次 HAL UART 错误快照。
 * @param uart UART 实例；当前仅有效枚举值可读取后端错误。
 * @retval PLATFORM_ERROR_INVALID_ARGUMENT uart 越界。
 * @post 合法实例返回软件错误位与 HAL 错误映射的按位或；非法实例不访问快照。
 * @note 只读，不清除软件或 HAL 错误；不停止传输、不修改缓冲区，不分配内存。
 */
uint32_t Platform_UART_GetLastError(
  platform_uart_instance_t uart
);

/**
 * @brief 注册 UART 回调。
 * @param uart UART 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param callbacks 待复制的回调集合；不得为 NULL，callback 成员可为 NULL 以禁用通知。
 * @pre 必须在启动 IT/DMA 传输前调用，且一个实例只能有一个回调所有者。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该实例已注册回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或回调集合非法。
 * @post 成功时复制 callback 和 context，后续完成、错误或中止事件调用该回调。
 * @warning 回调在中断上下文执行；事件对象只在回调期间有效，不得阻塞、分配内存或保存指针。
 * @note 每个实例仅允许成功注册一次；context 的生命周期由调用方维护。
 */
platform_status_t Platform_UART_RegisterCallback(
  platform_uart_instance_t uart,
  const platform_uart_callback_binding_t *callbacks
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_UART_H
