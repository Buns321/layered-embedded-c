//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file firewater_usart.h
 * @brief 通过 USART DMA 发送的 FireWater 文本遥测。
 * @addtogroup drivers_concrete
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_FIREWATER_USART_H
#define LAYERED_EMBEDDED_C_FIREWATER_USART_H

#include "telemetry_base.h"
#include "usart_base.h"
#include <stdbool.h>
#include <stdint.h>

#define FIREWATER_USART_TX_CAPACITY 512u

/**
 * @brief FireWater USART 设备对象。
 * @note Init 会取得该 USART 回调槽的所有权。该 USART 实例不得再有其他
 *       独立回调使用者。
 */
typedef struct {
  telemetry_base_t base;
  usart_base_t *usart;
  const char *const *channel_names;
  uint8_t channel_count;
  uint8_t precision;

  uint8_t tx_buffer[FIREWATER_USART_TX_CAPACITY];
  volatile bool busy;
  volatile uint32_t sent_frames;
  volatile uint32_t dropped_frames;
  volatile uint32_t error_count;
} firewater_usart_t;

/**
 * @brief 在 USART 上初始化 FireWater。
 * @param this 对象。
 * @param name 调试名称。
 * @param usart 已完成初始化且 TX 支持 DMA 的 USART 后端。
 * @param channel_names 静态通道名称表。
 * @param channel_count 已配置通道数，范围 1..capacity。
 * @param precision 小数位数，范围 0..6。
 */
void Dev_FireWater_USART_Init(
  firewater_usart_t *this,
  const char *name,
  usart_base_t *usart,
  const char *const channel_names[],
  uint8_t channel_count,
  uint8_t precision
);

/** @} */

#endif //LAYERED_EMBEDDED_C_FIREWATER_USART_H
