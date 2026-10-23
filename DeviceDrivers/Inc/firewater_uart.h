//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file firewater_uart.h
 * @brief 通过 UART DMA 发送的 FireWater 文本遥测。
 * @addtogroup drivers_concrete
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_FIREWATER_UART_H
#define LAYERED_EMBEDDED_C_FIREWATER_UART_H

#include "telemetry_base.h"
#include "uart_base.h"
#include <stdbool.h>
#include <stdint.h>

#define FIREWATER_UART_TX_CAPACITY 512u

/**
 * @brief FireWater UART 设备对象。
 * @note Init 会取得该 UART 回调槽的所有权。该 UART 实例不得再有其他
 *       独立回调使用者。
 */
typedef struct firewater_uart_s {
  telemetry_base_t base;
  uart_base_t *uart_h;
  const char *const *channel_names;
  uint8_t channel_count;
  uint8_t precision;

  uint8_t tx_buffer[FIREWATER_UART_TX_CAPACITY];
  volatile bool busy;
  volatile uint32_t sent_frames;
  volatile uint32_t dropped_frames;
  volatile uint32_t error_count;
} firewater_uart_t;

/**
 * @brief 在 UART 上初始化 FireWater。
 * @param telemetry_h 对象。
 * @param name 调试名称。
 * @param uart_h 已完成初始化且 TX 支持 DMA 的 UART 后端。
 * @param channel_names 静态通道名称表。
 * @param channel_count 已配置通道数，范围 1..capacity。
 * @param precision 小数位数，范围 0..6。
 */
void Dev_FireWater_UART_Init(
  firewater_uart_t *telemetry_h,
  const char *name,
  uart_base_t *uart_h,
  const char *const channel_names[],
  uint8_t channel_count,
  uint8_t precision
);

/** @} */

#endif //LAYERED_EMBEDDED_C_FIREWATER_UART_H
