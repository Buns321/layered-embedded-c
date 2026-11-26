/**
 * @file bsp_uart.c
 * @brief 静态 UART 服务实现与 STM32 HAL 中断回调分发。
 * @ingroup bsp
 */

#include "bsp_uart.h"
#include "bsp_hal_internal.h"
#include "sys_critical.h"
#include <stddef.h>
#include <string.h>

static bsp_uart_t *s_uart_slots[BSP_UART_MAX_INSTANCES];

static bsp_uart_t *bsp_uart_findbyhal(const UART_HandleTypeDef *huart) {
  if (huart == NULL) {
    return NULL;
  }
  for (uint32_t i = 0U; i < BSP_UART_MAX_INSTANCES; ++i) {
    bsp_uart_t *slot = s_uart_slots[i];
    if (slot != NULL && slot->registered && slot->huart == huart) {
      return slot;
    }
  }
  return NULL;
}

static void bsp_uart_pushslice(bsp_uart_t *uart, uint16_t start, uint16_t end) {
  for (uint16_t i = start; i < end; ++i) {
    uart->rx_bytes++;
    if (!Sys_ByteRing_PushFromISR(&uart->rx_ring, uart->rx_dma_buf[i])) {
      uart->rx_dropped_bytes++;
      uart->resync_requested = true;
    }
  }
}

static bsp_status_t bsp_uart_armrx(bsp_uart_t *uart) {
  if (uart->rx_mode == BSP_UART_RX_NONE) {
    uart->rearm_pending = false;
    return BSP_OK;
  }

  HAL_StatusTypeDef hal_st = HAL_ERROR;
  if (uart->rx_mode == BSP_UART_RX_IT_BYTE) {
    hal_st = HAL_UART_Receive_IT(uart->huart, &uart->rx_byte, 1U);
  } else if (uart->rx_mode == BSP_UART_RX_DMA_IDLE) {
    if (uart->huart->hdmarx == NULL) {
      uart->rearm_pending = true;
      return BSP_ERR_INVALID_ARG;
    }
    uart->rx_dma_last_pos = 0U;
    hal_st = HAL_UARTEx_ReceiveToIdle_DMA(uart->huart, uart->rx_dma_buf, uart->rx_dma_size);
    if (hal_st == HAL_OK) {
      __HAL_DMA_DISABLE_IT(uart->huart->hdmarx, DMA_IT_HT);
    }
  } else {
    return BSP_ERR_INVALID_ARG;
  }

  uart->rearm_pending = (hal_st != HAL_OK);
  return bsp_status_from_hal(hal_st);
}

bsp_status_t BSP_UART_Init(bsp_uart_t *uart, const bsp_uart_cfg_t *cfg) {
  if (uart == NULL || cfg == NULL || cfg->huart == NULL) {
    return BSP_ERR_INVALID_ARG;
  }
  if (cfg->rx_mode != BSP_UART_RX_NONE && cfg->rx_mode != BSP_UART_RX_IT_BYTE &&
      cfg->rx_mode != BSP_UART_RX_DMA_IDLE) {
    return BSP_ERR_INVALID_ARG;
  }
  if (cfg->rx_mode != BSP_UART_RX_NONE) {
    if (cfg->rx_ring_buf == NULL || cfg->rx_ring_size < 2U ||
        (cfg->rx_ring_size & (uint16_t)(cfg->rx_ring_size - 1U)) != 0U) {
      return BSP_ERR_INVALID_ARG;
    }
  }
  if (cfg->rx_mode == BSP_UART_RX_DMA_IDLE) {
    if (cfg->rx_dma_buf == NULL || cfg->rx_dma_size < 2U) {
      return BSP_ERR_INVALID_ARG;
    }
  }

  uint32_t free_idx = BSP_UART_MAX_INSTANCES;
  for (uint32_t i = 0U; i < BSP_UART_MAX_INSTANCES; ++i) {
    bsp_uart_t *slot = s_uart_slots[i];
    if (slot == uart || (slot != NULL && slot->huart == cfg->huart)) {
      return BSP_ERR_BUSY;
    }
    if (slot == NULL && free_idx == BSP_UART_MAX_INSTANCES) {
      free_idx = i;
    }
  }
  if (free_idx >= BSP_UART_MAX_INSTANCES) {
    return BSP_ERR_NO_MEM;
  }

  (void)memset(uart, 0, sizeof(*uart));
  if (cfg->rx_mode != BSP_UART_RX_NONE) {
    if (!Sys_ByteRing_Init(&uart->rx_ring, cfg->rx_ring_buf, cfg->rx_ring_size)) {
      return BSP_ERR_INVALID_ARG;
    }
  }

  uart->huart = cfg->huart;
  uart->rx_mode = cfg->rx_mode;
  uart->rx_dma_buf = cfg->rx_dma_buf;
  uart->rx_dma_size = cfg->rx_dma_size;
  uart->on_tx_done = cfg->on_tx_done;
  uart->tx_ctx = cfg->tx_ctx;
  uart->registered = true;

  Sys_Critical_Enter();
  s_uart_slots[free_idx] = uart;
  Sys_Critical_Exit();
  return BSP_OK;
}

bsp_status_t BSP_UART_StartRx(bsp_uart_t *uart) {
  if (uart == NULL || !uart->registered || uart->huart == NULL) {
    return BSP_ERR_INVALID_ARG;
  }
  return bsp_uart_armrx(uart);
}

void BSP_UART_EnsureRxArmed(bsp_uart_t *uart) {
  if (uart == NULL || !uart->registered || uart->huart == NULL ||
      uart->rx_mode == BSP_UART_RX_NONE) {
    return;
  }
  if (uart->rearm_pending) {
    (void)bsp_uart_armrx(uart);
  }
}

bool BSP_UART_PopByte(bsp_uart_t *uart, uint8_t *out_byte) {
  if (uart == NULL || !uart->registered || out_byte == NULL || uart->rx_mode == BSP_UART_RX_NONE) {
    return false;
  }
  return Sys_ByteRing_Pop(&uart->rx_ring, out_byte);
}

bool BSP_UART_TakeResync(bsp_uart_t *uart) {
  if (uart == NULL || !uart->registered) {
    return false;
  }
  bool resync = false;
  Sys_Critical_Enter();
  if (uart->resync_requested) {
    uart->resync_requested = false;
    resync = true;
  }
  Sys_Critical_Exit();
  return resync;
}

bsp_status_t BSP_UART_SendBlocking(bsp_uart_t *uart, const uint8_t *data, uint16_t len,
                                   uint32_t timeout_ms) {
  if (uart == NULL || !uart->registered || uart->huart == NULL || data == NULL || len == 0U) {
    return BSP_ERR_INVALID_ARG;
  }
  return bsp_status_from_hal(HAL_UART_Transmit(uart->huart, data, len, timeout_ms));
}

bsp_status_t BSP_UART_SendDMA(bsp_uart_t *uart, const uint8_t *data, uint16_t len) {
  if (uart == NULL || !uart->registered || uart->huart == NULL || data == NULL || len == 0U) {
    return BSP_ERR_INVALID_ARG;
  }
  if (BSP_UART_IsTxBusy(uart)) {
    return BSP_ERR_BUSY;
  }

  /* 先置忙标志，避免 HAL 启动后中断在返回前就触发 TxCpltCallback 时被误覆盖。 */
  uart->tx_busy = true;
  HAL_StatusTypeDef hal_st = HAL_UART_Transmit_DMA(uart->huart, data, len);
  if (hal_st != HAL_OK) {
    uart->tx_busy = false;
    return bsp_status_from_hal(hal_st);
  }
  return BSP_OK;
}

bool BSP_UART_IsTxBusy(const bsp_uart_t *uart) {
  if (uart == NULL || !uart->registered || uart->huart == NULL) {
    return false;
  }
  return uart->tx_busy ||
         ((uart->huart->gState & HAL_UART_STATE_BUSY_TX) == HAL_UART_STATE_BUSY_TX);
}

void BSP_UART_ResetSlots(void) {
  for (uint32_t i = 0U; i < BSP_UART_MAX_INSTANCES; ++i) {
    s_uart_slots[i] = NULL;
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  bsp_uart_t *uart = bsp_uart_findbyhal(huart);
  if (uart == NULL || uart->rx_mode != BSP_UART_RX_IT_BYTE) {
    return;
  }

  uart->rx_bytes++;
  if (!Sys_ByteRing_PushFromISR(&uart->rx_ring, uart->rx_byte)) {
    uart->rx_dropped_bytes++;
    uart->resync_requested = true;
  }
  (void)bsp_uart_armrx(uart);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
  bsp_uart_t *uart = bsp_uart_findbyhal(huart);
  if (uart == NULL || uart->rx_mode != BSP_UART_RX_DMA_IDLE) {
    return;
  }

  uint16_t dma_size = uart->rx_dma_size;
  uint16_t cur_pos = (Size > dma_size) ? dma_size : Size;
  uint16_t last_pos = uart->rx_dma_last_pos;

  if (cur_pos > last_pos) {
    bsp_uart_pushslice(uart, last_pos, cur_pos);
  } else if (cur_pos < last_pos) {
    bsp_uart_pushslice(uart, last_pos, dma_size);
    bsp_uart_pushslice(uart, 0U, cur_pos);
  }

  if (huart->hdmarx != NULL && huart->hdmarx->Init.Mode == DMA_CIRCULAR) {
    uart->rx_dma_last_pos = (cur_pos == dma_size) ? 0U : cur_pos;
  } else {
    uart->rx_dma_last_pos = 0U;
    (void)bsp_uart_armrx(uart);
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
  bsp_uart_t *uart = bsp_uart_findbyhal(huart);
  if (uart == NULL) {
    return;
  }

  uart->tx_busy = false;
  if (uart->on_tx_done != NULL) {
    uart->on_tx_done(uart->tx_ctx);
  }
}

void HAL_UART_AbortTransmitCpltCallback(UART_HandleTypeDef *huart) {
  bsp_uart_t *uart = bsp_uart_findbyhal(huart);
  if (uart == NULL) {
    return;
  }

  uart->tx_busy = false;
  if (uart->on_tx_done != NULL) {
    uart->on_tx_done(uart->tx_ctx);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  bsp_uart_t *uart = bsp_uart_findbyhal(huart);
  if (uart == NULL) {
    return;
  }

  __HAL_UART_CLEAR_OREFLAG(huart);
  __HAL_UART_CLEAR_FEFLAG(huart);
  __HAL_UART_CLEAR_NEFLAG(huart);
  __HAL_UART_CLEAR_PEFLAG(huart);

  if (uart->tx_busy) {
    uart->tx_busy = false;
    uart->tx_error_count++;
    if (uart->on_tx_done != NULL) {
      uart->on_tx_done(uart->tx_ctx);
    }
  }

  if (uart->rx_mode != BSP_UART_RX_NONE) {
    uart->rx_error_count++;
    uart->resync_requested = true;
    (void)bsp_uart_armrx(uart);
  }
}
