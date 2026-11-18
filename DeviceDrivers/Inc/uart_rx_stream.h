/**
 * @file uart_rx_stream.h
 * @brief UART 单字节中断接收、环形缓冲与重挂前端。
 * @addtogroup drivers
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_UART_RX_STREAM_H
#define LAYERED_EMBEDDED_C_UART_RX_STREAM_H

#include "system_byte_ring.h"
#include "uart_base.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief UART 单字节中断接收流对象。
 * @note 缓冲区由调用方静态提供并保持整个对象生命周期有效。
 * @note ISR 只调用 OnRxComplete / OnError，任务上下文调用 EnsureArmed / Pop / TakeResync。
 */
typedef struct uart_rx_stream_s {
  uart_base_t *uart_h;
  uint8_t rx_byte;
  sys_byte_ring_t rx_ring;
  volatile bool rearm_pending;
  volatile bool resync_requested;
  volatile uint32_t rx_byte_count;
  volatile uint32_t uart_error_count;
  volatile uint32_t rearm_error_count;
} uart_rx_stream_t;

/**
 * @brief 初始化 UART 接收流并绑定静态环形缓冲区。
 * @param stream_h 待初始化的接收流对象，不得为 NULL。
 * @param uart_h 已初始化的 UART 后端句柄，不得为 NULL；调用方保留所有权。
 * @param buffer 调用方静态持有的环形缓冲区，不得为 NULL。
 * @param buffer_size_bytes 缓冲区字节容量，单位 byte；必须大于 0。
 * @retval true 初始化成功，计数与标志已清零。
 * @retval false 参数无效或环形缓冲初始化失败。
 * @pre 只在任务初始化阶段、启动接收前调用。
 * @post stream_h 绑定 uart_h 与 rx_ring，所有计数器和标志清零。
 * @note 不注册 UART 回调，不启动接收，不分配动态内存。
 */
bool Dev_UartRxStream_Init(uart_rx_stream_t *stream_h, uart_base_t *uart_h, uint8_t *buffer,
                           uint16_t buffer_size_bytes);

/**
 * @brief 立即尝试挂接一次单字节中断接收。
 * @param stream_h 已初始化的接收流对象，不得为 NULL。
 * @return 后端 Dev_UART_ReceiveIT 的返回状态。
 * @retval DEV_UART_OK 挂接成功，rearm_pending 清为 false。
 * @retval DEV_UART_BUSY 后端忙，rearm_pending 置为 true 且 rearm_error_count 递增。
 * @retval DEV_UART_ERROR 后端错误，rearm_pending 置为 true 且 rearm_error_count 递增。
 * @pre stream_h 与 uart_h 非空；任务和 ISR 不得在同一时刻并发调用本函数。
 * @post rearm_pending 反映是否需要后续在任务上下文重试。
 * @note 可在任务启动阶段、ISR 回调末尾或 EnsureArmed 中调用。
 */
dev_uart_status_t Dev_UartRxStream_Rearm(uart_rx_stream_t *stream_h);

/**
 * @brief UART 接收完成中断处理：入队字节并重新挂接接收。
 * @param stream_h 已初始化的接收流对象，不得为 NULL。
 * @pre 仅在 UART RX 完成中断上下文调用；rx_byte 已由后端写入最新字节。
 * @post rx_byte_count 递增；入队失败时置位 resync_requested；已尝试重挂下一字节接收。
 * @note 不在 ISR 中执行协议解析。
 */
void Dev_UartRxStream_OnRxComplete(uart_rx_stream_t *stream_h);

/**
 * @brief UART 错误中断处理：记录错误、请求重同步并重新挂接接收。
 * @param stream_h 已初始化的接收流对象，不得为 NULL。
 * @pre 仅在 UART 错误中断上下文调用。
 * @post uart_error_count 递增，resync_requested 置为 true，已尝试重挂接收。
 * @note 重挂失败时置位 rearm_pending 留待任务侧 EnsureArmed 重试。
 */
void Dev_UartRxStream_OnError(uart_rx_stream_t *stream_h);

/**
 * @brief 若存在待重挂请求，在任务上下文重试挂接单字节接收。
 * @param stream_h 已初始化的接收流对象，不得为 NULL。
 * @pre 仅在任务上下文调用；不得与启动前初始化并发。
 * @post 若 rearm_pending 原为 true，则已尝试调用 Rearm 并更新该标志。
 * @note 无待重挂请求时立即返回，不触发后端调用。
 */
void Dev_UartRxStream_EnsureArmed(uart_rx_stream_t *stream_h);

/**
 * @brief 从环形缓冲弹出一个已接收字节。
 * @param stream_h 已初始化的接收流对象，不得为 NULL。
 * @param byte 输出字节指针，不得为 NULL。
 * @retval true 成功弹出 1 字节写入 *byte。
 * @retval false 环形缓冲为空，*byte 保持不变。
 * @pre 仅由单个消费者任务调用。
 * @post 非空时消费 1 字节。
 * @note 无锁 SPSC 读取，不阻塞。
 */
bool Dev_UartRxStream_Pop(uart_rx_stream_t *stream_h, uint8_t *byte);

/**
 * @brief 检查并清除 ISR 发出的重同步请求标志。
 * @param stream_h 已初始化的接收流对象，不得为 NULL。
 * @retval true 曾有溢出或 UART 错误触发重同步请求，且标志已被清除。
 * @retval false 当前无重同步请求。
 * @pre 仅由单个消费者任务调用。
 * @post 返回 true 时 resync_requested 已清零。
 * @note 不丢弃环形缓冲中已入队字节，仅通知上层协议状态机回到同步起点。
 */
bool Dev_UartRxStream_TakeResync(uart_rx_stream_t *stream_h);

/**
 * @brief 读取环形缓冲因满而丢弃的累计字节数。
 * @param stream_h 已初始化的接收流对象，不得为 NULL。
 * @return 累计丢弃字节数，单位 byte。
 * @post 不修改接收流状态。
 * @note 只增不减，溢出后自然回绕。
 */
uint32_t Dev_UartRxStream_GetDroppedBytes(const uart_rx_stream_t *stream_h);

/** @} */

#endif // LAYERED_EMBEDDED_C_UART_RX_STREAM_H
