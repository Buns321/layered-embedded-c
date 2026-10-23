//
// Created by Buns_ on 2026/10/23.
//

/**
 * @file platform_spi.h
 * @brief HAL-free SPI master transport interface.
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_SPI_H
#define LAYERED_EMBEDDED_C_PLATFORM_SPI_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

#if !PLATFORM_ENABLE_SPI1 && !PLATFORM_ENABLE_SPI2 && \
    !PLATFORM_ENABLE_SPI3
#error "At least one Platform SPI instance must be enabled"
#endif

/** @brief Available SPI instances. */
typedef enum {
#if PLATFORM_ENABLE_SPI1
  PLATFORM_SPI_1 = 0U,
#endif
#if PLATFORM_ENABLE_SPI2
  PLATFORM_SPI_2 = 1U,
#endif
#if PLATFORM_ENABLE_SPI3
  PLATFORM_SPI_3 = 2U,
#endif
} Platform_SPI_Instance;

/** @brief Number of possible SPI identity slots. */
#define PLATFORM_SPI_INSTANCE_COUNT 3U

/** @brief Data width configured by CubeMX for an SPI instance. */
typedef enum {
  PLATFORM_SPI_DATA_WIDTH_8_BIT = 8U,
  PLATFORM_SPI_DATA_WIDTH_16_BIT = 16U,
} Platform_SPI_DataWidth;

/** @brief SPI error flags. Multiple bits may be set together. */
typedef enum {
  PLATFORM_SPI_ERROR_NONE    = 0u,
  PLATFORM_SPI_ERROR_MODF    = (1u << 0),
  PLATFORM_SPI_ERROR_CRC     = (1u << 1),
  PLATFORM_SPI_ERROR_OVR     = (1u << 2),
  PLATFORM_SPI_ERROR_FRE     = (1u << 3),
  PLATFORM_SPI_ERROR_DMA     = (1u << 4),
  PLATFORM_SPI_ERROR_FLAG    = (1u << 5),
  PLATFORM_SPI_ERROR_ABORT   = (1u << 6),
  PLATFORM_SPI_ERROR_UNKNOWN = (1u << 7),
} Platform_SPI_Error;

typedef void (*Platform_SPI_Callback)(
  Platform_SPI_Instance spi,
  void *context
);

/**
 * @brief SPI completion callbacks.
 * @note Callbacks run in interrupt context.
 * @note One physical instance has one callback owner for its whole lifetime.
 */
typedef struct {
  Platform_SPI_Callback tx_cplt_cb;
  Platform_SPI_Callback rx_cplt_cb;
  Platform_SPI_Callback txrx_cplt_cb;
  Platform_SPI_Callback err_cb;
  Platform_SPI_Callback abort_cplt_cb;
  void *context;
} platform_spi_callbacks_t;

/**
 * @brief Read the data width configured by CubeMX.
 * @param spi SPI instance.
 * @note Byte counts in transfer functions refer to bytes even in 16-bit mode.
 */
Platform_SPI_DataWidth Platform_SPI_GetDataWidth(
  Platform_SPI_Instance spi
);

/**
 * @brief Blocking master transmit.
 * @param spi SPI instance.
 * @param data Buffer of size bytes. It is read only during the call.
 * @param size Byte count. Must be even in 16-bit mode.
 * @param timeout Timeout in milliseconds or PLATFORM_TIMEOUT_INFINITE.
 * @pre MX_SPIx_Init() completed and the required chip select is asserted by
 *      the caller.
 */
Platform_StatusTypeDef Platform_SPI_Transmit(
  Platform_SPI_Instance spi,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief Blocking master receive.
 * @param spi SPI instance.
 * @param data Receive buffer.
 * @param size Byte count. Must be even in 16-bit mode.
 * @param timeout Timeout in milliseconds or PLATFORM_TIMEOUT_INFINITE.
 */
Platform_StatusTypeDef Platform_SPI_Receive(
  Platform_SPI_Instance spi,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief Blocking full-duplex master transfer.
 * @param spi SPI instance.
 * @param tx_data Transmit buffer.
 * @param rx_data Receive buffer.
 * @param size Byte count. Must be even in 16-bit mode.
 * @param timeout Timeout in milliseconds or PLATFORM_TIMEOUT_INFINITE.
 */
Platform_StatusTypeDef Platform_SPI_TransmitReceive(
  Platform_SPI_Instance spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief Interrupt master transmit.
 * @param spi SPI instance.
 * @param data Transmit buffer.
 * @param size Byte count.
 * @note The buffer must remain valid until completion, error, or abort.
 */
Platform_StatusTypeDef Platform_SPI_Transmit_IT(
  Platform_SPI_Instance spi,
  const uint8_t *data,
  uint16_t size
);

/**
 * @brief Interrupt master receive.
 * @param spi SPI instance.
 * @param data Receive buffer.
 * @param size Byte count.
 * @note The buffer must remain valid until completion, error, or abort.
 */
Platform_StatusTypeDef Platform_SPI_Receive_IT(
  Platform_SPI_Instance spi,
  uint8_t *data,
  uint16_t size
);

/**
 * @brief Interrupt full-duplex master transfer.
 * @param spi SPI instance.
 * @param tx_data Transmit buffer.
 * @param rx_data Receive buffer.
 * @param size Byte count.
 * @note Both buffers must remain valid until completion, error, or abort.
 */
Platform_StatusTypeDef Platform_SPI_TransmitReceive_IT(
  Platform_SPI_Instance spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
);

/**
 * @brief DMA master transmit.
 * @param spi SPI instance.
 * @param data Transmit buffer.
 * @param size Byte count.
 * @note The buffer must remain valid until completion, error, or abort.
 */
Platform_StatusTypeDef Platform_SPI_Transmit_DMA(
  Platform_SPI_Instance spi,
  const uint8_t *data,
  uint16_t size
);

/**
 * @brief DMA master receive.
 * @param spi SPI instance.
 * @param data Receive buffer.
 * @param size Byte count.
 * @note The buffer must remain valid until completion, error, or abort.
 */
Platform_StatusTypeDef Platform_SPI_Receive_DMA(
  Platform_SPI_Instance spi,
  uint8_t *data,
  uint16_t size
);

/**
 * @brief DMA full-duplex master transfer.
 * @param spi SPI instance.
 * @param tx_data Transmit buffer.
 * @param rx_data Receive buffer.
 * @param size Byte count.
 * @note Both buffers must remain valid until completion, error, or abort.
 */
Platform_StatusTypeDef Platform_SPI_TransmitReceive_DMA(
  Platform_SPI_Instance spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
);

/**
 * @brief Abort the active transfer synchronously and clear its error snapshot.
 * @param spi SPI instance.
 * @note Call from task context, never from an ISR.
 */
Platform_StatusTypeDef Platform_SPI_Abort(Platform_SPI_Instance spi);

/**
 * @brief Abort the active transfer using interrupt mode.
 * @param spi SPI instance.
 * @note The abort completion callback runs in interrupt context.
 */
Platform_StatusTypeDef Platform_SPI_Abort_IT(Platform_SPI_Instance spi);

/**
 * @brief Read the latest HAL SPI error snapshot as Platform_SPI_Error bits.
 * @param spi SPI instance.
 */
uint32_t Platform_SPI_GetLastError(Platform_SPI_Instance spi);

/**
 * @brief Register SPI callbacks.
 * @param spi SPI instance.
 * @param callbacks Callback set to copy.
 * @note The structure is copied. Call once during initialization before
 *       starting IT/DMA transfers.
 */
void Platform_SPI_RegisterCallbacks(
  Platform_SPI_Instance spi,
  const platform_spi_callbacks_t *callbacks
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_SPI_H
