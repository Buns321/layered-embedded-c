//
// 由 Buns_ 创建于 2026/8/24。
//

/**
 * @file platform_usart.h
 * @brief USART 抽象：收发、IT/DMA、回调注册与错误清理。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_USART_H
#define LAYERED_EMBEDDED_C_PLATFORM_USART_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

/**
 * @brief UART 实例身份。
 * @note 未启用的实例不会进入该枚举。
 */
typedef enum {
#if PLATFORM_ENABLE_UART1
  PLATFORM_USART_1 = 0U,
#endif
#if PLATFORM_ENABLE_UART2
  PLATFORM_USART_2 = 1U,
#endif
#if PLATFORM_ENABLE_UART3
  PLATFORM_USART_3 = 2U,
#endif
#if PLATFORM_ENABLE_UART4
  PLATFORM_UART_4 = 3U,
#endif
#if PLATFORM_ENABLE_UART5
  PLATFORM_UART_5 = 4U,
#endif
#if PLATFORM_ENABLE_UART6
  PLATFORM_USART_6 = 5U,
#endif
} Platform_UART_Instance;

#if !PLATFORM_ENABLE_UART1 && !PLATFORM_ENABLE_UART2 && \
    !PLATFORM_ENABLE_UART3 && !PLATFORM_ENABLE_UART4 && \
    !PLATFORM_ENABLE_UART5 && !PLATFORM_ENABLE_UART6
#error "At least one Platform UART instance must be enabled"
#endif

typedef void (*Platform_UART_Callback)(Platform_UART_Instance uart, void *context);

/**
 * @brief Platform UART 回调集合。
 * @note 回调在中断上下文执行，回调内不要做阻塞操作。
 * @note 注册时结构体内容会被复制，调用方无需保持其生命周期。
 * @note 一个物理 UART 只有一个回调所有者。必须在启动传输前完成注册，
 *       不支持运行期并发替换回调集合。
 * @note context 的生命周期必须覆盖整个外设使用周期。
 */
typedef struct {
  Platform_UART_Callback tx_cplt_cb;
  Platform_UART_Callback tx_halfcplt_cb;
  Platform_UART_Callback rx_cplt_cb;
  Platform_UART_Callback rx_halfcplt_cb;
  Platform_UART_Callback err_cb;
  Platform_UART_Callback abort_cplt_cb;
  Platform_UART_Callback abort_txcplt_cb;
  Platform_UART_Callback abort_rxcplt_cb;
  void *context;
} platform_uart_callbacks_t;

/**
 * @brief UART 错误位。
 * @note 多个位可以同时置位。
 */
typedef enum {
  PLATFORM_UART_ERROR_NONE    = 0u,
  PLATFORM_UART_ERROR_PARITY  = (1u << 0),
  PLATFORM_UART_ERROR_NOISE   = (1u << 1),
  PLATFORM_UART_ERROR_FRAME   = (1u << 2),
  PLATFORM_UART_ERROR_OVERRUN = (1u << 3),
  PLATFORM_UART_ERROR_DMA     = (1u << 5),
  PLATFORM_UART_ERROR_UNKNOWN = (1u << 7),
} Platform_UART_Error;

/**
 * @brief 阻塞接收。
 * @param uart UART 实例。
 * @param pdata 接收缓冲区。
 * @param size 字节数，必须大于 0。
 * @param timeout 超时时间（ms），可为 PLATFORM_TIMEOUT_INFINITE。
 * @pre 对应 UART 的 MX_USARTx_UART_Init() 已完成。
 * @return Platform 状态码。
 */
Platform_StatusTypeDef Platform_UART_Receive(
  Platform_UART_Instance uart,
  uint8_t *pdata,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief 阻塞发送。
 * @param uart UART 实例。
 * @param pdata 发送缓冲区，调用期间只读。
 * @param size 字节数，必须大于 0。
 * @param timeout 超时时间（ms），可为 PLATFORM_TIMEOUT_INFINITE。
 * @pre 对应 UART 的 MX_USARTx_UART_Init() 已完成。
 * @return Platform 状态码。
 */
Platform_StatusTypeDef Platform_UART_Transmit(
  Platform_UART_Instance uart,
  const uint8_t *pdata,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief 中断接收。
 * @note pdata 必须保持有效，直到 RX 完成或错误回调。
 * @note 重新挂接收由设备驱动负责，Platform 不自动重挂。
 */
Platform_StatusTypeDef Platform_UART_Receive_IT(
  Platform_UART_Instance uart,
  uint8_t *pdata,
  uint16_t size
);

/**
 * @brief 中断发送。
 * @note pdata 必须保持有效，直到 TX 完成或错误回调。
 */
Platform_StatusTypeDef Platform_UART_Transmit_IT(
  Platform_UART_Instance uart,
  const uint8_t *pdata,
  uint16_t size
);

/**
 * @brief DMA 接收。
 * @note pdata 必须保持有效，直到 RX 完成或错误回调。
 * @note 半完成回调时只能访问已经完成的那一半数据。
 */
Platform_StatusTypeDef Platform_UART_Receive_DMA(
  Platform_UART_Instance uart,
  uint8_t *pdata,
  uint16_t size
);

/**
 * @brief DMA 发送。
 * @note pdata 必须保持有效，直到 TX 完成或错误回调。
 */
Platform_StatusTypeDef Platform_UART_Transmit_DMA(
  Platform_UART_Instance uart,
  const uint8_t *pdata,
  uint16_t size
);

/**
 * @brief 中止接收，清除 UART 硬件错误标志和 HAL 错误码。
 * @param uart UART 实例。
 * @retval Platform_OK 成功。
 * @retval Platform_ERROR 失败。
 */
Platform_StatusTypeDef Platform_UART_AbortReceive(
  Platform_UART_Instance uart
);

/**
 * @brief 中止发送并清除 HAL 错误码。
 * @param uart UART 实例。
 * @retval Platform_OK 成功。
 * @retval Platform_ERROR 失败。
 */
Platform_StatusTypeDef Platform_UART_AbortTransmit(
  Platform_UART_Instance uart
);

/**
 * @brief 中止发送和接收，并清除 UART 错误状态。
 * @param uart UART 实例。
 * @retval Platform_OK 成功。
 * @retval Platform_ERROR 失败。
 */
Platform_StatusTypeDef Platform_UART_Abort(
  Platform_UART_Instance uart
);

/**
 * @brief 兼容旧接口：中止接收并清除错误状态。
 * @deprecated 新代码应使用 Platform_UART_AbortReceive()。
 */
Platform_StatusTypeDef Platform_UART_ClearBuffer(
  Platform_UART_Instance uart
);

/**
 * @brief 读取最近一次 HAL UART 错误快照。
 * @return Platform_UART_Error 位掩码。
 */
uint32_t Platform_UART_GetLastError(
  Platform_UART_Instance uart
);

/**
 * @brief 注册 UART 回调。
 * @param uart UART 实例。
 * @param cb 回调集合，内容会被复制。
 * @pre 必须在启动 IT/DMA 传输前调用，且一个实例只能有一个回调所有者。
 */
void Platform_UART_RegisterCallbacks(
  Platform_UART_Instance uart,
  const platform_uart_callbacks_t *cb
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_USART_H
