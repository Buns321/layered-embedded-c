/**
 * @file platform_spi.c
 * @brief STM32F1 HAL backend for the common SPI implementation.
 */

#include "platform_hal_internal.h"
#include "platform_spi_backend.h"
#include "spi.h"
#include "stm32f1xx_hal.h"

static SPI_HandleTypeDef *platform_spi_to_hal(Platform_SPI_Instance spi) {
  switch (spi) {
#if PLATFORM_ENABLE_SPI1
    case PLATFORM_SPI_1: return &hspi1;
#endif
#if PLATFORM_ENABLE_SPI2
    case PLATFORM_SPI_2: return &hspi2;
#endif
    default: return NULL;
  }
}

static bool platform_hal_to_spi(
  SPI_HandleTypeDef *hspi,
  Platform_SPI_Instance *spi
) {
  if (hspi == NULL || spi == NULL) {
    return false;
  }
#if PLATFORM_ENABLE_SPI1
  if (hspi == &hspi1) {
    *spi = PLATFORM_SPI_1;
    return true;
  }
#endif
#if PLATFORM_ENABLE_SPI2
  if (hspi == &hspi2) {
    *spi = PLATFORM_SPI_2;
    return true;
  }
#endif
  return false;
}

static uint16_t platform_spi_hal_size(
  SPI_HandleTypeDef *hspi,
  uint16_t size
) {
  return hspi->Init.DataSize == SPI_DATASIZE_16BIT
           ? (uint16_t)(size / 2U)
           : size;
}

static uint32_t platform_spi_map_error(uint32_t hal_error) {
  uint32_t error = PLATFORM_SPI_ERROR_NONE;
  if ((hal_error & HAL_SPI_ERROR_MODF) != 0U) {
    error |= PLATFORM_SPI_ERROR_MODF;
  }
  if ((hal_error & HAL_SPI_ERROR_CRC) != 0U) {
    error |= PLATFORM_SPI_ERROR_CRC;
  }
  if ((hal_error & HAL_SPI_ERROR_OVR) != 0U) {
    error |= PLATFORM_SPI_ERROR_OVR;
  }
  if ((hal_error & HAL_SPI_ERROR_DMA) != 0U) {
    error |= PLATFORM_SPI_ERROR_DMA;
  }
  if ((hal_error & HAL_SPI_ERROR_FLAG) != 0U) {
    error |= PLATFORM_SPI_ERROR_FLAG;
  }
  if ((hal_error & HAL_SPI_ERROR_ABORT) != 0U) {
    error |= PLATFORM_SPI_ERROR_ABORT;
  }

  const uint32_t known_errors =
    HAL_SPI_ERROR_MODF |
    HAL_SPI_ERROR_CRC |
    HAL_SPI_ERROR_OVR |
    HAL_SPI_ERROR_DMA |
    HAL_SPI_ERROR_FLAG |
    HAL_SPI_ERROR_ABORT;
  if ((hal_error & ~known_errors) != 0U) {
    error |= PLATFORM_SPI_ERROR_UNKNOWN;
  }
  return error;
}

bool platform_spi_backend_is_valid(Platform_SPI_Instance spi) {
  return (uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT &&
         platform_spi_to_hal(spi) != NULL;
}

Platform_StatusTypeDef platform_spi_backend_get_data_width(
  Platform_SPI_Instance spi,
  Platform_SPI_DataWidth *data_width
) {
  if (data_width == NULL) {
    return Platform_INVALID_ARGUMENT;
  }
  SPI_HandleTypeDef *h = platform_spi_to_hal(spi);
  if (h == NULL) {
    return Platform_INVALID_ARGUMENT;
  }
  if (h->Init.DataSize == SPI_DATASIZE_16BIT) {
    *data_width = PLATFORM_SPI_DATA_WIDTH_16_BIT;
    return Platform_OK;
  }
  if (h->Init.DataSize == SPI_DATASIZE_8BIT) {
    *data_width = PLATFORM_SPI_DATA_WIDTH_8_BIT;
    return Platform_OK;
  }
  return Platform_INVALID_ARGUMENT;
}

Platform_StatusTypeDef platform_spi_backend_transmit(
  Platform_SPI_Instance spi,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  SPI_HandleTypeDef *h = platform_spi_to_hal(spi);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_Transmit(
                       h,
                       (uint8_t *)data,
                       platform_spi_hal_size(h, size),
                       timeout
                     ));
}

Platform_StatusTypeDef platform_spi_backend_receive(
  Platform_SPI_Instance spi,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  SPI_HandleTypeDef *h = platform_spi_to_hal(spi);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_Receive(
                       h,
                       data,
                       platform_spi_hal_size(h, size),
                       timeout
                     ));
}

Platform_StatusTypeDef platform_spi_backend_transmit_receive(
  Platform_SPI_Instance spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size,
  uint32_t timeout
) {
  SPI_HandleTypeDef *h = platform_spi_to_hal(spi);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_TransmitReceive(
                       h,
                       (uint8_t *)tx_data,
                       rx_data,
                       platform_spi_hal_size(h, size),
                       timeout
                     ));
}

Platform_StatusTypeDef platform_spi_backend_transmit_it(
  Platform_SPI_Instance spi,
  const uint8_t *data,
  uint16_t size
) {
  SPI_HandleTypeDef *h = platform_spi_to_hal(spi);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_Transmit_IT(
                       h,
                       (uint8_t *)data,
                       platform_spi_hal_size(h, size)
                     ));
}

Platform_StatusTypeDef platform_spi_backend_receive_it(
  Platform_SPI_Instance spi,
  uint8_t *data,
  uint16_t size
) {
  SPI_HandleTypeDef *h = platform_spi_to_hal(spi);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_Receive_IT(
                       h,
                       data,
                       platform_spi_hal_size(h, size)
                     ));
}

Platform_StatusTypeDef platform_spi_backend_transmit_receive_it(
  Platform_SPI_Instance spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
) {
  SPI_HandleTypeDef *h = platform_spi_to_hal(spi);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_TransmitReceive_IT(
                       h,
                       (uint8_t *)tx_data,
                       rx_data,
                       platform_spi_hal_size(h, size)
                     ));
}

Platform_StatusTypeDef platform_spi_backend_transmit_dma(
  Platform_SPI_Instance spi,
  const uint8_t *data,
  uint16_t size
) {
  SPI_HandleTypeDef *h = platform_spi_to_hal(spi);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_Transmit_DMA(
                       h,
                       (uint8_t *)data,
                       platform_spi_hal_size(h, size)
                     ));
}

Platform_StatusTypeDef platform_spi_backend_receive_dma(
  Platform_SPI_Instance spi,
  uint8_t *data,
  uint16_t size
) {
  SPI_HandleTypeDef *h = platform_spi_to_hal(spi);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_Receive_DMA(
                       h,
                       data,
                       platform_spi_hal_size(h, size)
                     ));
}

Platform_StatusTypeDef platform_spi_backend_transmit_receive_dma(
  Platform_SPI_Instance spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
) {
  SPI_HandleTypeDef *h = platform_spi_to_hal(spi);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_TransmitReceive_DMA(
                       h,
                       (uint8_t *)tx_data,
                       rx_data,
                       platform_spi_hal_size(h, size)
                     ));
}

Platform_StatusTypeDef platform_spi_backend_abort(Platform_SPI_Instance spi) {
  SPI_HandleTypeDef *h = platform_spi_to_hal(spi);
  if (h == NULL) {
    return Platform_INVALID_ARGUMENT;
  }
  HAL_StatusTypeDef status = HAL_SPI_Abort(h);
  if (status == HAL_OK) {
    h->ErrorCode = HAL_SPI_ERROR_NONE;
  }
  return platform_from_hal_status(status);
}

Platform_StatusTypeDef platform_spi_backend_abort_it(
  Platform_SPI_Instance spi
) {
  SPI_HandleTypeDef *h = platform_spi_to_hal(spi);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_Abort_IT(h));
}

uint32_t platform_spi_backend_get_error(Platform_SPI_Instance spi) {
  SPI_HandleTypeDef *h = platform_spi_to_hal(spi);
  return h == NULL ? PLATFORM_SPI_ERROR_UNKNOWN
                   : platform_spi_map_error(HAL_SPI_GetError(h));
}

static void platform_spi_dispatch_hal_event(
  SPI_HandleTypeDef *hspi,
  Platform_SPI_EventType type
) {
  Platform_SPI_Instance spi;
  if (platform_hal_to_spi(hspi, &spi)) {
    platform_spi_dispatch_event(spi, type);
  }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
  platform_spi_dispatch_hal_event(hspi, PLATFORM_SPI_EVENT_TX_CPLT);
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
  platform_spi_dispatch_hal_event(hspi, PLATFORM_SPI_EVENT_RX_CPLT);
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
  platform_spi_dispatch_hal_event(hspi, PLATFORM_SPI_EVENT_TXRX_CPLT);
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
  platform_spi_dispatch_hal_event(hspi, PLATFORM_SPI_EVENT_ERROR);
}

void HAL_SPI_AbortCpltCallback(SPI_HandleTypeDef *hspi) {
  platform_spi_dispatch_hal_event(hspi, PLATFORM_SPI_EVENT_ABORT_CPLT);
}
