/**
 * @file bsp_uart.h
 * @brief 静态 UART 服务：支持单字节 IT / DMA+IDLE 接收、环形缓冲、错误重挂与 DMA 发送。
 * @details 通过固定长度静态槽位表（`BSP_UART_MAX_INSTANCES`）将 STM32 HAL UART 中断/DMA 回调
 *          路由到对应的 `bsp_uart_t` 实例，结合单生产者-单消费者（SPSC）无锁字节环形缓冲与
 *          硬件错误自恢复重挂机制，实现零动态分配的异步串口收发服务。
 *
 * @addtogroup BSP
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
/**
 * @brief 全局静态 UART 实例槽位上限（无量纲）。
 * @details STM32F407VET6 最多提供 6 路 USART/UART，默认槽位数 6 可覆盖全片串口而无需动态分配。
 */
#define BSP_UART_MAX_INSTANCES 6U
#endif

/**
 * @brief UART 接收工作模式枚举。
 */
typedef enum bsp_uart_rx_mode_e {
  BSP_UART_RX_NONE = 0U, /**< 仅发送模式，不启动接收也不需要接收环形缓冲区。 */
  BSP_UART_RX_IT_BYTE, /**< 单字节中断接收模式（HAL_UART_Receive_IT），适合低波特率或字节流协议。 */
  BSP_UART_RX_DMA_IDLE, /**< DMA +
                           空闲中断接收模式（HAL_UARTEx_ReceiveToIdle_DMA），适合高速不定长帧。 */
} bsp_uart_rx_mode_t;

/**
 * @brief UART 异步发送完成或中止的 ISR 回调函数指针类型。
 * @param[in] ctx 注册时传入的用户上下文指针（由调用方决定是否可为 NULL）。
 * @warning 回调在 UART/DMA 中断上下文执行，严禁在回调内调用阻塞延时、信号量等待或耗时浮点运算。
 */
typedef void (*bsp_uart_tx_done_cb_t)(void *ctx);

/**
 * @brief UART 实例初始化配置结构体。
 * @details 所有缓冲区均由调用方静态持有并须覆盖 `bsp_uart_t` 的完整生命周期。
 */
typedef struct bsp_uart_cfg_s {
  UART_HandleTypeDef *huart;  /**< CubeMX 生成的 UART HAL 句柄指针（不允许为 NULL）。 */
  bsp_uart_rx_mode_t rx_mode; /**< 接收工作模式。 */
  uint8_t *rx_ring_buf;       /**< 接收环形缓冲区指针（`rx_mode != BSP_UART_RX_NONE` 时必填）。 */
  uint16_t rx_ring_size;      /**< 接收环形缓冲区容量（单位：byte，须为 >= 2 的 2 的幂）。 */
  uint8_t *rx_dma_buf;        /**< DMA 接收暂存缓冲区指针（仅 `BSP_UART_RX_DMA_IDLE` 时必填）。 */
  uint16_t rx_dma_size;       /**< DMA 接收缓冲区容量（单位：byte，须 >= 2）。 */
  bsp_uart_tx_done_cb_t on_tx_done; /**< 可选的 TX 完成/中止 ISR 回调函数指针（可为 NULL）。 */
  void *tx_ctx;                     /**< 透传给 `on_tx_done` 的用户上下文指针（可为 NULL）。 */
} bsp_uart_cfg_t;

/**
 * @brief UART 运行期实例对象。
 * @warning 须由调用方静态分配，严禁按值拷贝（内部地址已登记在全局槽位表且供 ISR 访问）。
 */
typedef struct bsp_uart_s {
  UART_HandleTypeDef *huart;  /**< 绑定的 UART HAL 句柄指针。 */
  bsp_uart_rx_mode_t rx_mode; /**< 当前接收模式。 */
  sys_byte_ring_t rx_ring;    /**< ISR 写、任务读的单生产者-单消费者（SPSC）字节环形缓冲。 */
  uint8_t rx_byte;            /**< 单字节中断模式（IT_BYTE）使用的 1 字节接收暂存区。 */
  uint8_t *rx_dma_buf;        /**< DMA+IDLE 模式使用的接收缓冲区指针。 */
  uint16_t rx_dma_size;       /**< DMA 接收缓冲区总长度（单位：byte）。 */
  uint16_t
      rx_dma_last_pos; /**< DMA 环形模式下已搬运至 `rx_ring` 的上次尾指针偏移（单位：byte）。 */
  bsp_uart_tx_done_cb_t on_tx_done; /**< TX 完成/中止 ISR 回调。 */
  void *tx_ctx;                     /**< 透传给 `on_tx_done` 的上下文指针。 */
  volatile bool tx_busy;            /**< 异步发送忙标志（ISR 清除，任务置位）。 */
  volatile bool
      rearm_pending; /**< 接收重挂失败标志；置位时由 BSP_UART_EnsureRxArmed() 在任务侧补偿。 */
  volatile bool resync_requested; /**< 帧重同步请求标志（环形缓冲溢出或硬件串口错误时置位）。 */
  volatile uint32_t rx_bytes;     /**< 累计已接收字节总数（单位：byte，含溢出丢弃前计入的字节）。 */
  volatile uint32_t rx_dropped_bytes; /**< 因 `rx_ring` 满而丢弃的累计字节数（单位：byte）。 */
  volatile uint32_t rx_error_count;   /**< 累计接收错误次数（ORE/FE/NE/PE）。 */
  volatile uint32_t tx_error_count;   /**< 累计发送错误次数。 */
  bool registered;                    /**< 实例已成功注册到全局静态槽位表标志。 */
} bsp_uart_t;

/**
 * @brief 注册并初始化静态 UART 实例。
 * @param[out] uart UART 运行期实例指针（不允许为 NULL）。
 * @param[in]  cfg  初始化配置指针（不允许为 NULL）。
 * @return 操作状态码。
 * @retval BSP_OK              初始化并注册成功。
 * @retval BSP_ERR_INVALID_ARG 参数为空、模式非法、环形缓冲非 >= 2 的 2 的幂或 DMA 缓冲长度不足。
 * @retval BSP_ERR_BUSY        `uart` 实例或 `cfg->huart` 已被重复注册。
 * @retval BSP_ERR_NO_MEM      全局静态槽位已满（超过 `BSP_UART_MAX_INSTANCES`）。
 * @pre 须在 CubeMX `MX_USARTx_UART_Init()` 完成后调用；不可与同一 `huart` 重复注册。
 */
bsp_status_t BSP_UART_Init(bsp_uart_t *uart, const bsp_uart_cfg_t *cfg);

/**
 * @brief 启动配置模式的异步接收。
 * @param[in,out] uart 已注册的 UART 实例指针（不允许为 NULL）。
 * @return 操作状态码。
 * @retval BSP_OK              接收启动成功（或模式为 `BSP_UART_RX_NONE`）。
 * @retval BSP_ERR_INVALID_ARG 实例未注册、句柄为空或 DMA 模式缺少 `hdmarx`。
 * @retval BSP_ERR_BUSY        HAL 接收外设正忙。
 * @retval BSP_ERR_HAL         底层 HAL 接收启动失败（同时置位 `rearm_pending`）。
 * @note 仅限任务上下文调用。
 */
bsp_status_t BSP_UART_StartRx(bsp_uart_t *uart);

/**
 * @brief 任务侧接收补偿重挂。
 * @param[in,out] uart UART 实例指针（允许为 NULL，无效或 `RX_NONE` 时安全忽略）。
 * @note 当 ISR 中重挂接收失败导致 `rearm_pending` 置位时，由周期任务调用本函数重新挂接接收。
 */
void BSP_UART_EnsureRxArmed(bsp_uart_t *uart);

/**
 * @brief 从接收 SPSC 环形缓冲弹出一个字节。
 * @param[in,out] uart     UART 实例指针（不允许为 NULL）。
 * @param[out]    out_byte 输出字节指针（不允许为 NULL）。
 * @return 是否成功弹出字节。
 * @retval true  成功读取 1 字节并写入 `*out_byte`。
 * @retval false 缓冲区为空、实例无效或接收模式为 `BSP_UART_RX_NONE`。
 * @warning 仅限单消费者任务上下文调用，不可多任务或 ISR 并发弹出。
 */
bool BSP_UART_PopByte(bsp_uart_t *uart, uint8_t *out_byte);

/**
 * @brief 原子读取并清除接收重同步请求标志。
 * @param[in,out] uart UART 实例指针（允许为 NULL，未注册或 NULL 时返回 false）。
 * @return 调用前是否挂起过重同步请求。
 * @retval true  曾发生环形缓冲溢出或硬件串口错误，上层解析器应丢弃半帧并重新对齐帧头。
 * @retval false 无挂起的重同步请求。
 * @note 内部通过临界区原子读取并清零 `resync_requested`。
 */
bool BSP_UART_TakeResync(bsp_uart_t *uart);

/**
 * @brief 以轮询阻塞方式发送数据。
 * @param[in] uart       已注册的 UART 实例指针（不允许为 NULL）。
 * @param[in] data       待发送数据指针（不允许为 NULL）。
 * @param[in] len        发送长度（单位：byte，须 > 0）。
 * @param[in] timeout_ms 阻塞超时时间（单位：ms）。
 * @return 操作状态码。
 * @retval BSP_OK              发送完成。
 * @retval BSP_ERR_INVALID_ARG 参数非法或实例未注册。
 * @retval BSP_ERR_BUSY        HAL 发送器正忙。
 * @retval BSP_ERR_TIMEOUT     阻塞发送超时。
 * @retval BSP_ERR_HAL         底层 HAL 发送出错。
 * @note 仅限任务上下文调用，禁止在 ISR 中使用。
 */
bsp_status_t BSP_UART_SendBlocking(bsp_uart_t *uart, const uint8_t *data, uint16_t len,
                                   uint32_t timeout_ms);

/**
 * @brief 以 DMA 异步方式发送数据。
 * @param[in,out] uart 已注册的 UART 实例指针（不允许为 NULL）。
 * @param[in]     data 待发送数据指针（不允许为 NULL）。
 * @param[in]     len  发送长度（单位：byte，须 > 0）。
 * @return 操作状态码。
 * @retval BSP_OK              DMA 发送已成功提交。
 * @retval BSP_ERR_INVALID_ARG 参数非法或实例未注册。
 * @retval BSP_ERR_BUSY        上一次发送尚未完成（`BSP_UART_IsTxBusy()` 为 true）。
 * @retval BSP_ERR_HAL         底层 HAL_UART_Transmit_DMA() 返回失败。
 * @warning `data` 指向的内存必须在 `on_tx_done` 回调触发或 `BSP_UART_IsTxBusy()` 返回 false
 * 前保持有效且不被修改。
 */
bsp_status_t BSP_UART_SendDMA(bsp_uart_t *uart, const uint8_t *data, uint16_t len);

/**
 * @brief 查询当前是否仍有异步/HAL 发送正在进行。
 * @param[in] uart UART 实例指针（允许为 NULL，无效时返回 false）。
 * @return 发送忙状态。
 * @retval true  内部 `tx_busy` 为 true 或 HAL `gState` 包含 `HAL_UART_STATE_BUSY_TX`。
 * @retval false 发送器空闲或实例无效。
 */
bool BSP_UART_IsTxBusy(const bsp_uart_t *uart);

/**
 * @brief 清空全局静态 UART 实例槽位表。
 * @warning 仅供主机单元测试在用例间重置静态状态使用，固件运行期严禁调用。
 */
void BSP_UART_ResetSlots(void);

#endif /* LAYERED_CAR_BSP_UART_H */

/** @} */
