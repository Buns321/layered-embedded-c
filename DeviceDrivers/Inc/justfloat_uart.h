//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file justfloat_uart.h
 * @brief 通过 UART DMA 发送的 JustFloat 二进制遥测。
 * @addtogroup drivers_concrete
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_JUSTFLOAT_UART_H
#define LAYERED_EMBEDDED_C_JUSTFLOAT_UART_H

#include "telemetry_base.h"
#include "uart_base.h"
#include <stdbool.h>
#include <stdint.h>

#define JUSTFLOAT_FRAME_TAIL_SIZE 4u
#define JUSTFLOAT_UART_TX_CAPACITY \
  ((TELEMETRY_CHANNEL_CAPACITY * 4u) + JUSTFLOAT_FRAME_TAIL_SIZE)

/**
 * @brief JustFloat UART 设备对象。
 * @note Init 会取得该 UART 回调槽的所有权。该 UART 实例不得再有其他
 *       独立回调使用者。
 */
typedef struct justfloat_uart_s {
  telemetry_base_t base;
  uart_base_t *uart_h;
  uint8_t channel_count;

  uint8_t tx_buffer[JUSTFLOAT_UART_TX_CAPACITY];
  volatile bool busy;
  volatile uint32_t sent_frames;
  volatile uint32_t dropped_frames;
  volatile uint32_t error_count;
} justfloat_uart_t;

/**
 * @brief 在 UART 上初始化 JustFloat。
 * @param telemetry_h 对象。
 * @param name 调试名称。
 * @param uart_h 已完成初始化且 TX 支持 DMA 的 UART 后端。
 * @param channel_count 每帧通道数，范围 1..capacity。
 */
void Dev_JustFloat_UART_Init(
  justfloat_uart_t *telemetry_h,
  const char *name,
  uart_base_t *uart_h,
  uint8_t channel_count
);

/** @} */

#endif //LAYERED_EMBEDDED_C_JUSTFLOAT_UART_H
