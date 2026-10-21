//
// Created by Buns_ on 2026/10/21.
//

/**
 * @file justfloat_usart.h
 * @brief JustFloat binary telemetry over a USART DMA transmitter.
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
 * @brief JustFloat USART device object.
 * @note Init takes ownership of the USART callback slot. The USART instance
 *       must not have another independent callback client.
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
 * @brief Initialize JustFloat over USART.
 * @param this Object.
 * @param name Debug name.
 * @param usart Initialized USART backend with DMA-capable TX.
 * @param channel_count Number of channels in every frame, 1..capacity.
 */
void Dev_JustFloat_USART_Init(
  justfloat_usart_t *this,
  const char *name,
  usart_base_t *usart,
  uint8_t channel_count
);

/** @} */

#endif //LAYERED_EMBEDDED_C_JUSTFLOAT_USART_H
