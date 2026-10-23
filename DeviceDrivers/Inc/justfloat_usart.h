//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file justfloat_usart.h
 * @brief 通过 USART DMA 发送的 JustFloat 二进制遥测。
 * @addtogroup drivers_concrete
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_JUSTFLOAT_USART_H
#define LAYERED_EMBEDDED_C_JUSTFLOAT_USART_H

#include "telemetry_base.h"
#include "usart_base.h"
#include <stdbool.h>
#include <stdint.h>

#define JUSTFLOAT_FRAME_TAIL_SIZE 4u
#define JUSTFLOAT_USART_TX_CAPACITY \
  ((TELEMETRY_CHANNEL_CAPACITY * 4u) + JUSTFLOAT_FRAME_TAIL_SIZE)

/**
 * @brief JustFloat USART 设备对象。
 * @note Init 会取得该 USART 回调槽的所有权。该 USART 实例不得再有其他
 *       独立回调使用者。
 */
typedef struct {
  telemetry_base_t base;
  usart_base_t *usart;
  uint8_t channel_count;

  uint8_t tx_buffer[JUSTFLOAT_USART_TX_CAPACITY];
  volatile bool busy;
  volatile uint32_t sent_frames;
  volatile uint32_t dropped_frames;
  volatile uint32_t error_count;
} justfloat_usart_t;

/**
 * @brief 在 USART 上初始化 JustFloat。
 * @param this 对象。
 * @param name 调试名称。
 * @param usart 已完成初始化且 TX 支持 DMA 的 USART 后端。
 * @param channel_count 每帧通道数，范围 1..capacity。
 */
void Dev_JustFloat_USART_Init(
  justfloat_usart_t *this,
  const char *name,
  usart_base_t *usart,
  uint8_t channel_count
);

/** @} */

#endif //LAYERED_EMBEDDED_C_JUSTFLOAT_USART_H
