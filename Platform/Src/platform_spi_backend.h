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

bool platform_spi_backend_is_valid(Platform_SPI_Instance spi);

Platform_StatusTypeDef platform_spi_backend_get_data_width(
  Platform_SPI_Instance spi,
  Platform_SPI_DataWidth *data_width
);

Platform_StatusTypeDef platform_spi_backend_transmit(
  Platform_SPI_Instance spi,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

Platform_StatusTypeDef platform_spi_backend_receive(
  Platform_SPI_Instance spi,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

Platform_StatusTypeDef platform_spi_backend_transmit_receive(
  Platform_SPI_Instance spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size,
  uint32_t timeout
);

Platform_StatusTypeDef platform_spi_backend_transmit_it(
  Platform_SPI_Instance spi,
  const uint8_t *data,
  uint16_t size
);

Platform_StatusTypeDef platform_spi_backend_receive_it(
  Platform_SPI_Instance spi,
  uint8_t *data,
  uint16_t size
);

Platform_StatusTypeDef platform_spi_backend_transmit_receive_it(
  Platform_SPI_Instance spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
);

Platform_StatusTypeDef platform_spi_backend_transmit_dma(
  Platform_SPI_Instance spi,
  const uint8_t *data,
  uint16_t size
);

Platform_StatusTypeDef platform_spi_backend_receive_dma(
  Platform_SPI_Instance spi,
  uint8_t *data,
  uint16_t size
);

Platform_StatusTypeDef platform_spi_backend_transmit_receive_dma(
  Platform_SPI_Instance spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
);

Platform_StatusTypeDef platform_spi_backend_abort(Platform_SPI_Instance spi);

Platform_StatusTypeDef platform_spi_backend_abort_it(
  Platform_SPI_Instance spi
);

uint32_t platform_spi_backend_get_error(Platform_SPI_Instance spi);

void platform_spi_dispatch_event(
  Platform_SPI_Instance spi,
  Platform_SPI_EventType type
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_SPI_BACKEND_H
