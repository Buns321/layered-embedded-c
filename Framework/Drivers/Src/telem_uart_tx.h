/**
 * @file telem_uart_tx.h
 * @brief 内部共用 UART DMA 遥测发送状态机与计数辅助接口。
 * @details 属于 Framework/Drivers 内部实现辅助头文件，供 telem_justfloat 与
 *          telem_firewater 共用，封装纯发送模式 BSP_UART 初始化、ISR 完成回调
 *          计数更新以及临界区状态快照读取。
 * @ingroup Framework_Drivers
 */

#ifndef LAYERED_CAR_TELEM_UART_TX_H
#define LAYERED_CAR_TELEM_UART_TX_H

#include "bsp_def.h"
#include "bsp_uart.h"
#include "telemetry_base.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 初始化纯发送模式 UART 实例与遥测发送状态结构体。
 * @param[out] uart  BSP UART 实例指针（非 NULL）。
 * @param[out] tx    遥测发送状态结构体指针（非 NULL）。
 * @param[in]  huart STM32 HAL UART 句柄指针（非 NULL）。
 * @return 初始化状态码。
 * @retval BSP_OK              初始化成功并绑定 TX 完成回调。
 * @retval BSP_ERR_INVALID_ARG 参数为空指针。
 * @retval 其他 bsp_status_t   底层 BSP_UART_Init 返回的错误码。
 */
bsp_status_t Telem_UartTx_Init(bsp_uart_t *uart, telemetry_tx_state_t *tx,
                               UART_HandleTypeDef *huart);

/**
 * @brief 检查发送通道是否空闲；若正忙则递增丢帧计数并返回 false。
 * @param[in]     uart BSP UART 实例指针（非 NULL）。
 * @param[in,out] tx   遥测发送状态结构体指针（非 NULL）。
 * @return 是否允许开始本帧编码与发送。
 * @retval true  通道空闲，可继续填充发送缓冲区。
 * @retval false 参数无效或通道正忙（已递增 dropped_frames）。
 */
bool Telem_UartTx_TryBegin(const bsp_uart_t *uart, telemetry_tx_state_t *tx);

/**
 * @brief 记录一次编码或参数阶段错误计数。
 * @param[in,out] tx 遥测发送状态结构体指针（为 NULL 时安全忽略）。
 */
void Telem_UartTx_RecordError(telemetry_tx_state_t *tx);

/**
 * @brief 提交一帧完整数据到 UART DMA 异步发送。
 * @param[in,out] uart BSP UART 实例指针（非 NULL）。
 * @param[in,out] tx   遥测发送状态结构体指针（非 NULL）。
 * @param[in]     buf  待发送数据缓冲区（须位于 DMA 可访问 SRAM，非 NULL）。
 * @param[in]     len  发送字节长度（单位：bytes，须 > 0）。
 * @return 提交状态码。
 * @retval DEV_TELEMETRY_OK    已成功启动 DMA 发送，等待 ISR 完成回调清忙。
 * @retval DEV_TELEMETRY_BUSY  底层硬件仍忙，已递增 dropped_frames 并清软件忙标志。
 * @retval DEV_TELEMETRY_ERROR 参数无效或底层 DMA 启动失败。
 */
dev_telemetry_status_t Telem_UartTx_SubmitDMA(bsp_uart_t *uart, telemetry_tx_state_t *tx,
                                              const uint8_t *buf, uint16_t len);

/**
 * @brief 临界区内读取遥测异步发送状态与统计计数快照。
 * @param[in]  uart   BSP UART 实例指针（为 NULL 时安全忽略）。
 * @param[in]  tx     遥测发送状态结构体指针（为 NULL 时安全忽略）。
 * @param[out] status 输出状态结构体指针（为 NULL 时安全忽略）。
 */
void Telem_UartTx_GetStatus(const bsp_uart_t *uart, const telemetry_tx_state_t *tx,
                            telemetry_status_t *status);

#endif /* LAYERED_CAR_TELEM_UART_TX_H */
