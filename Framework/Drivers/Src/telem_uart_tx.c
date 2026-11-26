/**
 * @file telem_uart_tx.c
 * @brief 内部共用 UART DMA 遥测发送状态机实现。
 * @ingroup drivers
 */

#include "telem_uart_tx.h"
#include "sys_critical.h"
#include <stddef.h>
#include <string.h>

static void telem_uart_tx_ontxdone(void *ctx) {
  telemetry_tx_state_t *tx = (telemetry_tx_state_t *)ctx;
  if (tx == NULL) {
    return;
  }

  uint32_t cur_err = (tx->uart_tx_err_ptr != NULL) ? *tx->uart_tx_err_ptr : 0U;
  if (cur_err != tx->last_uart_tx_err) {
    tx->last_uart_tx_err = cur_err;
    tx->error_count++;
  } else {
    tx->sent_frames++;
  }
  tx->busy = false;
}

bsp_status_t Telem_UartTx_Init(bsp_uart_t *uart, telemetry_tx_state_t *tx,
                               UART_HandleTypeDef *huart) {
  if (uart == NULL || tx == NULL || huart == NULL) {
    return BSP_ERR_INVALID_ARG;
  }

  (void)memset(tx, 0, sizeof(*tx));
  tx->uart_tx_err_ptr = &uart->tx_error_count;

  const bsp_uart_cfg_t uart_cfg = {
      .huart = huart,
      .rx_mode = BSP_UART_RX_NONE,
      .rx_ring_buf = NULL,
      .rx_ring_size = 0U,
      .rx_dma_buf = NULL,
      .rx_dma_size = 0U,
      .on_tx_done = telem_uart_tx_ontxdone,
      .tx_ctx = tx,
  };
  return BSP_UART_Init(uart, &uart_cfg);
}

bool Telem_UartTx_TryBegin(const bsp_uart_t *uart, telemetry_tx_state_t *tx) {
  if (uart == NULL || tx == NULL) {
    return false;
  }
  if (tx->busy || BSP_UART_IsTxBusy(uart)) {
    tx->dropped_frames++;
    return false;
  }
  return true;
}

void Telem_UartTx_RecordError(telemetry_tx_state_t *tx) {
  if (tx != NULL) {
    tx->error_count++;
  }
}

dev_telemetry_status_t Telem_UartTx_SubmitDMA(bsp_uart_t *uart, telemetry_tx_state_t *tx,
                                              const uint8_t *buf, uint16_t len) {
  if (uart == NULL || tx == NULL || buf == NULL || len == 0U) {
    return DEV_TELEMETRY_ERROR;
  }

  tx->busy = true;
  bsp_status_t st = BSP_UART_SendDMA(uart, buf, len);
  if (st != BSP_OK) {
    tx->busy = false;
    if (st == BSP_ERR_BUSY) {
      tx->dropped_frames++;
      return DEV_TELEMETRY_BUSY;
    }
    tx->error_count++;
    return DEV_TELEMETRY_ERROR;
  }
  return DEV_TELEMETRY_OK;
}

void Telem_UartTx_GetStatus(const bsp_uart_t *uart, const telemetry_tx_state_t *tx,
                            telemetry_status_t *status) {
  if (uart == NULL || tx == NULL || status == NULL) {
    return;
  }
  Sys_Critical_Enter();
  *status = (telemetry_status_t){
      .is_busy = tx->busy || BSP_UART_IsTxBusy(uart),
      .sent_frames = tx->sent_frames,
      .dropped_frames = tx->dropped_frames,
      .error_count = tx->error_count,
  };
  Sys_Critical_Exit();
}
