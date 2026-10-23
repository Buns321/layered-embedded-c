/**
 * @file platform_spi_backend.h
 * @brief Private STM32-family backend for the common SPI implementation.
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_SPI_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_SPI_BACKEND_H

#include "platform_def.h"
#include "platform_spi.h"
#include <stdbool.h>
#include <stdint.h>

bool platform_spi_backend_is_valid(platform_spi_instance_t spi);

platform_status_t platform_spi_backend_get_data_width(
  platform_spi_instance_t spi,
  platform_spi_data_width_t *data_width
);

platform_status_t platform_spi_backend_transmit(
  platform_spi_instance_t spi,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

platform_status_t platform_spi_backend_receive(
  platform_spi_instance_t spi,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

platform_status_t platform_spi_backend_transmit_receive(
  platform_spi_instance_t spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size,
  uint32_t timeout
);

platform_status_t platform_spi_backend_transmit_it(
  platform_spi_instance_t spi,
  const uint8_t *data,
  uint16_t size
);

platform_status_t platform_spi_backend_receive_it(
  platform_spi_instance_t spi,
  uint8_t *data,
  uint16_t size
);

platform_status_t platform_spi_backend_transmit_receive_it(
  platform_spi_instance_t spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
);

platform_status_t platform_spi_backend_transmit_dma(
  platform_spi_instance_t spi,
  const uint8_t *data,
  uint16_t size
);

platform_status_t platform_spi_backend_receive_dma(
  platform_spi_instance_t spi,
  uint8_t *data,
  uint16_t size
);

platform_status_t platform_spi_backend_transmit_receive_dma(
  platform_spi_instance_t spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
);

platform_status_t platform_spi_backend_abort(platform_spi_instance_t spi);

platform_status_t platform_spi_backend_abort_it(
  platform_spi_instance_t spi
);

uint32_t platform_spi_backend_get_error(platform_spi_instance_t spi);

void platform_spi_dispatch_event(
  platform_spi_instance_t spi,
  platform_spi_event_type_t type
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_SPI_BACKEND_H
