/**
 * @file telem_uart_tx.c
 * @brief 内部共用 UART DMA 遥测发送状态机实现。
 * @details 属于 Framework/Drivers 设备驱动层，实现 DMA TX 完成中断回调中的
 *          成功/错误计数同步与软件忙标志清除，以及任务侧非阻塞提交与临界区状态读取。
 * @ingroup Framework_Drivers
 */

#include "telem_uart_tx.h"
#include "sys_critical.h"
#include <stddef.h>
#include <string.h>

/**
 * @brief UART DMA 发送完成中断回调：根据底层错误计数增量更新统计并清除忙标志。
 * @param[in,out] ctx 指向 telemetry_tx_state_t 的上下文指针。
 * @note 本回调运行在 HAL_UART_TxCpltCallback / ErrorCallback 中断上下文，
 *       先更新统计字段再清除 volatile busy 标志，保证任务侧观测顺序一致。
 */
static void telem_uart_tx_ontxdone(void *ctx) {
  telemetry_tx_state_t *tx = (telemetry_tx_state_t *)ctx;
  if (tx == NULL) {
    return;
  }

  // 通过比对 bsp_uart_t::tx_error_count 增量判断本次传输是否发生 HAL 错误回调
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

  // 在启动 DMA 前先置位 busy，防止极短帧在 SendDMA 返回前即触发完成 ISR 导致清忙竞态
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
  // 临界区读取 ISR 回调与任务侧共享的发送统计计数，防止读到半更新快照
  Sys_Critical_Enter();
  *status = (telemetry_status_t){
      .is_busy = tx->busy || BSP_UART_IsTxBusy(uart),
      .sent_frames = tx->sent_frames,
      .dropped_frames = tx->dropped_frames,
      .error_count = tx->error_count,
  };
  Sys_Critical_Exit();
}
