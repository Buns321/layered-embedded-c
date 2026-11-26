/**
 * @file bsp_uart.h
 * @brief 静态 UART 服务：支持单字节 IT / DMA+IDLE 接收、环形缓冲、错误重挂与 DMA 发送。
 * @addtogroup bsp
 * @{
 */

#ifndef LAYERED_CAR_BSP_UART_H
#define LAYERED_CAR_BSP_UART_H

#include "bsp_def.h"
#include "bsp_mcu.h"
#include "sys_byte_ring.h"
#include <stdbool.h>
#include <stdint.h>

#ifndef BSP_UART_MAX_INSTANCES
#define BSP_UART_MAX_INSTANCES 6U
#endif

/**
 * @brief UART 接收工作模式。
 */
typedef enum bsp_uart_rx_mode_e {
  BSP_UART_RX_NONE = 0U, /* 仅发送 */
  BSP_UART_RX_IT_BYTE,   /* 单字节中断入队 */
  BSP_UART_RX_DMA_IDLE,  /* ReceiveToIdle DMA 块入队 */
} bsp_uart_rx_mode_t;

/**
 * @brief TX 完成或中止的 ISR 回调类型；不得在回调内执行阻塞操作。
 */
typedef void (*bsp_uart_tx_done_cb_t)(void *ctx);

/**
 * @brief UART 初始化配置；所有缓冲区由调用方静态持有并覆盖对象全生命周期。
 */
typedef struct bsp_uart_cfg_s {
  UART_HandleTypeDef *huart;
  bsp_uart_rx_mode_t rx_mode;
  uint8_t *rx_ring_buf;             /* rx_mode != NONE 时必填，容量须为 >= 2 的 2 的幂 */
  uint16_t rx_ring_size;            /* 单位 byte */
  uint8_t *rx_dma_buf;              /* 仅 RX_DMA_IDLE 时必填，>= 2 */
  uint16_t rx_dma_size;             /* 单位 byte */
  bsp_uart_tx_done_cb_t on_tx_done; /* 可选，TX 完成/中止 ISR 回调 */
  void *tx_ctx;
} bsp_uart_cfg_t;

/**
 * @brief UART 运行期实例对象；禁止按值拷贝。
 */
typedef struct bsp_uart_s {
  UART_HandleTypeDef *huart;
  bsp_uart_rx_mode_t rx_mode;
  sys_byte_ring_t rx_ring;
  uint8_t rx_byte;
  uint8_t *rx_dma_buf;
  uint16_t rx_dma_size;
  uint16_t rx_dma_last_pos;
  bsp_uart_tx_done_cb_t on_tx_done;
  void *tx_ctx;
  volatile bool tx_busy;
  volatile bool rearm_pending;
  volatile bool resync_requested;
  volatile uint32_t rx_bytes;
  volatile uint32_t rx_dropped_bytes;
  volatile uint32_t rx_error_count;
  volatile uint32_t tx_error_count;
  bool registered;
} bsp_uart_t;

/**
 * @brief 注册并初始化静态 UART 实例；重复注册同 huart 返回 BSP_ERR_BUSY，槽满返回 BSP_ERR_NO_MEM。
 */
bsp_status_t BSP_UART_Init(bsp_uart_t *uart, const bsp_uart_cfg_t *cfg);

/**
 * @brief 启动配置模式的接收（仅任务上下文调用）。
 */
bsp_status_t BSP_UART_StartRx(bsp_uart_t *uart);

/**
 * @brief 任务侧补偿重挂：若 ISR 重挂失败则重新启动接收。
 */
void BSP_UART_EnsureRxArmed(bsp_uart_t *uart);

/**
 * @brief 从 SPSC 环形缓冲弹出一个字节（仅单消费者任务上下文调用）。
 */
bool BSP_UART_PopByte(bsp_uart_t *uart, uint8_t *out_byte);

/**
 * @brief 原子读取并清除重同步请求标志（环形溢出或串口错误时置位）。
 */
bool BSP_UART_TakeResync(bsp_uart_t *uart);

/**
 * @brief 阻塞方式发送数据，timeout_ms 单位 ms（仅任务上下文调用）。
 */
bsp_status_t BSP_UART_SendBlocking(bsp_uart_t *uart, const uint8_t *data, uint16_t len,
                                   uint32_t timeout_ms);

/**
 * @brief DMA 异步发送；data 必须在 on_tx_done 或 !IsTxBusy 前保持有效。
 */
bsp_status_t BSP_UART_SendDMA(bsp_uart_t *uart, const uint8_t *data, uint16_t len);

/**
 * @brief 查询当前是否仍有 DMA/HAL 发送正在进行。
 */
bool BSP_UART_IsTxBusy(const bsp_uart_t *uart);

/**
 * @brief 仅用于主机单测：清空全局静态实例槽位表。
 */
void BSP_UART_ResetSlots(void);

/** @} */

#endif /* LAYERED_CAR_BSP_UART_H */
