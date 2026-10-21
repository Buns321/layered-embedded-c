//
// Created by Buns_ on 2026/10/21.
//

/**
 * @file firewater_usart.h
 * @brief FireWater text telemetry over a USART DMA transmitter.
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
 * @brief FireWater USART device object.
 * @note Init takes ownership of the USART callback slot. The USART instance
 *       must not have another independent callback client.
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
 * @brief Initialize FireWater over USART.
 * @param this Object.
 * @param name Debug name.
 * @param usart Initialized USART backend with DMA-capable TX.
 * @param channel_names Static channel-name table.
 * @param channel_count Number of configured channels, 1..capacity.
 * @param precision Decimal digits, 0..6.
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
