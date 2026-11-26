/**
 * @file bsp_uart.c
 * @brief 静态 UART 服务实现与 STM32 HAL 中断回调分发。
 * @details 维护固定容量的全局槽位表以将 HAL_UART_* 弱回调分发至对应 `bsp_uart_t` 实例，
 *          支持单字节 IT 与 DMA+IDLE（Circular/Normal）接收、SPSC 环形缓冲入队、
 *          错误标志清理与断链自动重挂。
 * @ingroup BSP
 */

#include "bsp_uart.h"
#include "bsp_hal_internal.h"
#include "sys_critical.h"
#include <stddef.h>
#include <string.h>

/** @brief 全局静态 UART 实例路由槽位表，供 HAL 中断回调按 `huart` 反查实例。 */
static bsp_uart_t *s_uart_slots[BSP_UART_MAX_INSTANCES];

/**
 * @brief 根据 STM32 HAL UART 句柄指针查找已注册的 `bsp_uart_t` 实例。
 * @param[in] huart HAL UART 句柄指针。
 * @return 匹配的已注册实例指针；未找到或 `huart == NULL` 时返回 NULL。
 */
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

/**
 * @brief 在 ISR 中将 DMA 接收缓冲区 `[start, end)` 切片逐字节推入 SPSC 环形缓冲。
 * @param[in,out] uart  UART 实例指针。
 * @param[in]     start 切片起始下标（含）。
 * @param[in]     end   切片结束下标（不含）。
 */
static void bsp_uart_pushslice(bsp_uart_t *uart, uint16_t start, uint16_t end) {
  for (uint16_t i = start; i < end; ++i) {
    uart->rx_bytes++;
    // 若任务侧消费不及时导致环形缓冲满，记录丢包并置位重同步请求以便协议层丢弃残帧
    if (!Sys_ByteRing_PushFromISR(&uart->rx_ring, uart->rx_dma_buf[i])) {
      uart->rx_dropped_bytes++;
      uart->resync_requested = true;
    }
  }
}

/**
 * @brief 按实例配置的接收模式挂接（或重挂）底层 HAL 接收请求。
 * @param[in,out] uart UART 实例指针。
 * @return 操作状态码。
 * @retval BSP_OK              挂接成功或当前为 `BSP_UART_RX_NONE`。
 * @retval BSP_ERR_INVALID_ARG DMA 模式未绑定 `hdmarx` 或接收模式非法。
 * @retval BSP_ERR_BUSY        HAL 接收外设正忙（同时置位 `rearm_pending` 供任务补偿）。
 * @retval BSP_ERR_HAL         底层 HAL 接收启动失败（同时置位 `rearm_pending`）。
 */
static bsp_status_t bsp_uart_armrx(bsp_uart_t *uart) {
  if (uart->rx_mode == BSP_UART_RX_NONE) {
    uart->rearm_pending = false;
    return BSP_OK;
  }

  HAL_StatusTypeDef hal_st = HAL_ERROR;
  if (uart->rx_mode == BSP_UART_RX_IT_BYTE) {
    // 单字节中断模式每次仅请求 1 字节，完成回调中需立即重挂下一字节
    hal_st = HAL_UART_Receive_IT(uart->huart, &uart->rx_byte, 1U);
  } else if (uart->rx_mode == BSP_UART_RX_DMA_IDLE) {
    if (uart->huart->hdmarx == NULL) {
      uart->rearm_pending = true;
      return BSP_ERR_INVALID_ARG;
    }
    uart->rx_dma_last_pos = 0U;
    hal_st = HAL_UARTEx_ReceiveToIdle_DMA(uart->huart, uart->rx_dma_buf, uart->rx_dma_size);
    if (hal_st == HAL_OK) {
      // 禁用 DMA 半传输（HT）中断，避免大帧在半缓冲处被额外拆分为两次无效回调
      __HAL_DMA_DISABLE_IT(uart->huart->hdmarx, DMA_IT_HT);
    }
  } else {
    return BSP_ERR_INVALID_ARG;
  }

  // 若 ISR 中因 HAL_BUSY 等瞬态原因重挂失败，置位 rearm_pending 由任务侧 EnsureRxArmed 兜底
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
    // SPSC 环形缓冲依赖位与掩码取模，容量必须为 >= 2 的 2 的幂
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

  // 扫描全局槽位表，禁止同一实例或同一 huart 句柄重复注册，并寻找首个空闲槽位
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

  // 在临界区内发布槽位指针，确保在任意并发中断到来前实例字段已完全初始化
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
  // 关中断临界区实现原子 read-and-clear，避免清除瞬间丢失 ISR 新置位的重同步请求
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

  // 先置忙标志，避免 HAL 启动后中断在返回前就触发 TxCpltCallback 时被误覆盖
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

/**
 * @brief STM32 HAL 单字节接收完成中断回调（覆写 HAL 弱符号）。
 * @param[in] huart 触发接收完成中断的 HAL UART 句柄指针。
 * @note 运行于 UART ISR 上下文；将收到字节推入 SPSC 环形缓冲后立即重挂下一次 IT 接收。
 */
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
  // 必须立即重挂单字节接收，否则后续到达字节将无法触发中断
  (void)bsp_uart_armrx(uart);
}

/**
 * @brief STM32 HAL ReceiveToIdle 接收事件中断回调（覆写 HAL 弱符号）。
 * @param[in] huart 触发 DMA/IDLE 事件的 HAL UART 句柄指针。
 * @param[in] Size  HAL 报告的当前 DMA 缓冲区累计写入偏移（单位：byte）。
 * @note 运行于 UART/DMA ISR 上下文；Circular 模式下按环形回绕搬运增量切片，Normal
 * 模式下搬运后重挂。
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
  bsp_uart_t *uart = bsp_uart_findbyhal(huart);
  if (uart == NULL || uart->rx_mode != BSP_UART_RX_DMA_IDLE) {
    return;
  }

  uint16_t dma_size = uart->rx_dma_size;
  uint16_t cur_pos = (Size > dma_size) ? dma_size : Size;
  uint16_t last_pos = uart->rx_dma_last_pos;

  // 若 cur_pos < last_pos，说明 Circular DMA 已发生缓冲区回绕，分两段搬运至 rx_ring
  if (cur_pos > last_pos) {
    bsp_uart_pushslice(uart, last_pos, cur_pos);
  } else if (cur_pos < last_pos) {
    bsp_uart_pushslice(uart, last_pos, dma_size);
    bsp_uart_pushslice(uart, 0U, cur_pos);
  }

  if (huart->hdmarx != NULL && huart->hdmarx->Init.Mode == DMA_CIRCULAR) {
    // Circular 模式由硬件自动回卷，仅更新软件尾指针（满缓冲时归零）
    uart->rx_dma_last_pos = (cur_pos == dma_size) ? 0U : cur_pos;
  } else {
    // Normal 模式下 DMA 传输已停止，需归零偏移并重新挂接接收
    uart->rx_dma_last_pos = 0U;
    (void)bsp_uart_armrx(uart);
  }
}

/**
 * @brief STM32 HAL 发送完成中断回调（覆写 HAL 弱符号）。
 * @param[in] huart 触发 TC 发送完成中断的 HAL UART 句柄指针。
 * @note 运行于 UART ISR 上下文；清除 `tx_busy` 并通知上层 `on_tx_done` 释放或复用发送缓冲。
 */
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

/**
 * @brief STM32 HAL 发送中止完成中断回调（覆写 HAL 弱符号）。
 * @param[in] huart 触发发送中止回调的 HAL UART 句柄指针。
 * @note 运行于 ISR 上下文；清除 `tx_busy` 并执行 `on_tx_done` 避免发送状态机永久卡死。
 */
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

/**
 * @brief STM32 HAL 串口错误中断回调（覆写 HAL 弱符号）。
 * @param[in] huart 发生 ORE/FE/NE/PE 错误的 HAL UART 句柄指针。
 * @note 运行于 UART ISR 上下文；清除硬件错误标志，恢复卡住的发送状态，置位重同步并重挂接收。
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  bsp_uart_t *uart = bsp_uart_findbyhal(huart);
  if (uart == NULL) {
    return;
  }

  // 显式清除溢出(ORE)/帧错误(FE)/噪声(NE)/奇偶校验(PE)标志，防止串口持续触发错误中断
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
    // 硬件出错后 HAL 会中止当前接收，必须重新挂接接收以恢复字节流输入
    (void)bsp_uart_armrx(uart);
  }
}
