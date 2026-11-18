/**
 * @file telemetry_uart_tx.h
 * @brief UART DMA 遥测发送状态机与计数公共前端。
 * @addtogroup drivers
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_TELEMETRY_UART_TX_H
#define LAYERED_EMBEDDED_C_TELEMETRY_UART_TX_H

#include "telemetry_base.h"
#include "uart_base.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief UART DMA 遥测发送状态机对象。
 * @note tx_buffer 由具体协议对象静态持有并保证在 DMA 完成前保持有效。
 */
typedef struct telemetry_uart_tx_s {
  uart_base_t *uart_h;
  uint8_t *tx_buffer;
  uint16_t tx_capacity_bytes;
  volatile bool busy;
  volatile uint32_t sent_frames;
  volatile uint32_t dropped_frames;
  volatile uint32_t error_count;
} telemetry_uart_tx_t;

/**
 * @brief 初始化 UART 遥测发送状态机并接管 UART TX 完成回调槽。
 * @param tx_h 待初始化的发送状态机，不得为 NULL。
 * @param uart_h 已初始化且支持 TX DMA 的 UART 后端，不得为 NULL；调用方保留所有权。
 * @param tx_buffer 调用方静态持有的发送缓冲区，不得为 NULL。
 * @param tx_capacity_bytes 发送缓冲区字节容量，单位 byte；必须大于 0。
 * @pre 只在单线程初始化阶段调用，uart_h 当前无进行中传输且无其他回调所有者。
 * @post tx_h 已清零并绑定缓冲区，uart_h 回调槽已注册内部 TX 完成回调。
 * @note 不启动传输、不阻塞、不分配动态内存。
 * @warning 注册回调会覆盖 uart_h 原有回调槽。
 */
void Dev_TelemetryUartTx_Init(telemetry_uart_tx_t *tx_h, uart_base_t *uart_h, uint8_t *tx_buffer,
                              uint16_t tx_capacity_bytes);

/**
 * @brief 检查发送通道是否空闲；若忙则计入丢帧。
 * @param tx_h 已初始化的发送状态机，不得为 NULL。
 * @retval true 发送通道空闲，调用方可安全填充 tx_buffer 并调用 SubmitDMA。
 * @retval false 上一帧仍在发送，dropped_frames 已自增 1。
 * @pre 仅允许单个发送任务调用。
 * @post 返回 false 时 dropped_frames 递增；不修改 tx_buffer。
 * @note 不阻塞、不等待 DMA。
 */
bool Dev_TelemetryUartTx_TryBegin(telemetry_uart_tx_t *tx_h);

/**
 * @brief 记录一次协议配置或编码错误。
 * @param tx_h 已初始化的发送状态机，不得为 NULL。
 * @post error_count 自增 1。
 * @note 仅在编码/校验失败且未启动 DMA 时调用。
 */
void Dev_TelemetryUartTx_RecordError(telemetry_uart_tx_t *tx_h);

/**
 * @brief 提交已编码的帧到 UART DMA 并统一映射忙/错误状态。
 * @param tx_h 已初始化的发送状态机，不得为 NULL。
 * @param frame_size_bytes 已写入 tx_buffer 的帧长度，单位 byte；不得超过容量。
 * @retval DEV_TELEMETRY_OK 帧已提交 DMA，完成前 busy 保持 true。
 * @retval DEV_TELEMETRY_BUSY UART 后端忙，busy 已恢复 false 且 dropped_frames 递增。
 * @retval DEV_TELEMETRY_ERROR UART 后端错误，busy 已恢复 false 且 error_count 递增。
 * @pre tx_buffer[0..frame_size_bytes-1] 已编码完成，且 TryBegin 返回 true。
 * @post 成功时 busy 为 true；失败时 busy 为 false 并更新对应计数器。
 * @note 不阻塞等待传输完成。
 */
dev_telemetry_status_t Dev_TelemetryUartTx_SubmitDMA(telemetry_uart_tx_t *tx_h,
                                                     uint16_t frame_size_bytes);

/**
 * @brief 在临界区内读取遥测发送状态快照。
 * @param tx_h 已初始化的发送状态机，不得为 NULL。
 * @param status 输出状态缓冲区，不得为 NULL。
 * @pre 仅在任务上下文调用。
 * @post status 写入一致的 is_busy、sent_frames、dropped_frames 与 error_count。
 * @note 不修改状态机或计数器。
 */
void Dev_TelemetryUartTx_GetStatus(const telemetry_uart_tx_t *tx_h, telemetry_status_t *status);

/** @} */

#endif // LAYERED_EMBEDDED_C_TELEMETRY_UART_TX_H
