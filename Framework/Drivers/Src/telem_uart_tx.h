/**
 * @file telem_uart_tx.h
 * @brief 内部共用 UART DMA 遥测发送状态机与计数辅助。
 * @ingroup drivers
 */

#ifndef LAYERED_CAR_TELEM_UART_TX_H
#define LAYERED_CAR_TELEM_UART_TX_H

#include "bsp_def.h"
#include "bsp_uart.h"
#include "telemetry_base.h"
#include <stdbool.h>
#include <stdint.h>

bsp_status_t Telem_UartTx_Init(bsp_uart_t *uart, telemetry_tx_state_t *tx,
                               UART_HandleTypeDef *huart);

bool Telem_UartTx_TryBegin(const bsp_uart_t *uart, telemetry_tx_state_t *tx);

void Telem_UartTx_RecordError(telemetry_tx_state_t *tx);

dev_telemetry_status_t Telem_UartTx_SubmitDMA(bsp_uart_t *uart, telemetry_tx_state_t *tx,
                                              const uint8_t *buf, uint16_t len);

void Telem_UartTx_GetStatus(const bsp_uart_t *uart, const telemetry_tx_state_t *tx,
                            telemetry_status_t *status);

#endif /* LAYERED_CAR_TELEM_UART_TX_H */
