/**
 * @file platform_spi.c
 * @brief STM32F1 公共 SPI 实现的 HAL 后端。
 * @ingroup platform
 */

#include "platform_hal_internal.h"
#include "platform_spi_backend.h"
#include "spi.h"
#include "stm32f1xx_hal.h"

static SPI_HandleTypeDef *platform_spi_to_hal(platform_spi_instance_t spi) {
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
  SPI_HandleTypeDef *spi_h,
  platform_spi_instance_t *spi
) {
  if (spi_h == NULL || spi == NULL) {
    return false;
  }
#if PLATFORM_ENABLE_SPI1
  if (spi_h == &hspi1) {
    *spi = PLATFORM_SPI_1;
    return true;
  }
#endif
#if PLATFORM_ENABLE_SPI2
  if (spi_h == &hspi2) {
    *spi = PLATFORM_SPI_2;
    return true;
  }
#endif
  return false;
}

static uint16_t platform_spi_hal_size(
  SPI_HandleTypeDef *spi_h,
  uint16_t size
) {
  return spi_h->Init.DataSize == SPI_DATASIZE_16BIT
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

bool platform_spi_backend_is_valid(platform_spi_instance_t spi) {
  return (uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT &&
         platform_spi_to_hal(spi) != NULL;
}

platform_status_t platform_spi_backend_get_data_width(
  platform_spi_instance_t spi,
  platform_spi_data_width_t *data_width
) {
  if (data_width == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  SPI_HandleTypeDef *spi_h = platform_spi_to_hal(spi);
  if (spi_h == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (spi_h->Init.DataSize == SPI_DATASIZE_16BIT) {
    *data_width = PLATFORM_SPI_DATA_WIDTH_16_BIT;
    return PLATFORM_STATUS_OK;
  }
  if (spi_h->Init.DataSize == SPI_DATASIZE_8BIT) {
    *data_width = PLATFORM_SPI_DATA_WIDTH_8_BIT;
    return PLATFORM_STATUS_OK;
  }
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

platform_status_t platform_spi_backend_transmit(
  platform_spi_instance_t spi,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  SPI_HandleTypeDef *spi_h = platform_spi_to_hal(spi);
  return spi_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_Transmit(
                       spi_h,
                       (uint8_t *)data,
                       platform_spi_hal_size(spi_h, size),
                       timeout
                     ));
}

platform_status_t platform_spi_backend_receive(
  platform_spi_instance_t spi,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  SPI_HandleTypeDef *spi_h = platform_spi_to_hal(spi);
  return spi_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_Receive(
                       spi_h,
                       data,
                       platform_spi_hal_size(spi_h, size),
                       timeout
                     ));
}

platform_status_t platform_spi_backend_transmit_receive(
  platform_spi_instance_t spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size,
  uint32_t timeout
) {
  SPI_HandleTypeDef *spi_h = platform_spi_to_hal(spi);
  return spi_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_TransmitReceive(
                       spi_h,
                       (uint8_t *)tx_data,
                       rx_data,
                       platform_spi_hal_size(spi_h, size),
                       timeout
                     ));
}

platform_status_t platform_spi_backend_transmit_it(
  platform_spi_instance_t spi,
  const uint8_t *data,
  uint16_t size
) {
  SPI_HandleTypeDef *spi_h = platform_spi_to_hal(spi);
  return spi_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_Transmit_IT(
                       spi_h,
                       (uint8_t *)data,
                       platform_spi_hal_size(spi_h, size)
                     ));
}

platform_status_t platform_spi_backend_receive_it(
  platform_spi_instance_t spi,
  uint8_t *data,
  uint16_t size
) {
  SPI_HandleTypeDef *spi_h = platform_spi_to_hal(spi);
  return spi_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_Receive_IT(
                       spi_h,
                       data,
                       platform_spi_hal_size(spi_h, size)
                     ));
}

platform_status_t platform_spi_backend_transmit_receive_it(
  platform_spi_instance_t spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
) {
  SPI_HandleTypeDef *spi_h = platform_spi_to_hal(spi);
  return spi_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_TransmitReceive_IT(
                       spi_h,
                       (uint8_t *)tx_data,
                       rx_data,
                       platform_spi_hal_size(spi_h, size)
                     ));
}

platform_status_t platform_spi_backend_transmit_dma(
  platform_spi_instance_t spi,
  const uint8_t *data,
  uint16_t size
) {
  SPI_HandleTypeDef *spi_h = platform_spi_to_hal(spi);
  return spi_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_Transmit_DMA(
                       spi_h,
                       (uint8_t *)data,
                       platform_spi_hal_size(spi_h, size)
                     ));
}

platform_status_t platform_spi_backend_receive_dma(
  platform_spi_instance_t spi,
  uint8_t *data,
  uint16_t size
) {
  SPI_HandleTypeDef *spi_h = platform_spi_to_hal(spi);
  return spi_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_Receive_DMA(
                       spi_h,
                       data,
                       platform_spi_hal_size(spi_h, size)
                     ));
}

platform_status_t platform_spi_backend_transmit_receive_dma(
  platform_spi_instance_t spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
) {
  SPI_HandleTypeDef *spi_h = platform_spi_to_hal(spi);
  return spi_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_TransmitReceive_DMA(
                       spi_h,
                       (uint8_t *)tx_data,
                       rx_data,
                       platform_spi_hal_size(spi_h, size)
                     ));
}

platform_status_t platform_spi_backend_abort(platform_spi_instance_t spi) {
  SPI_HandleTypeDef *spi_h = platform_spi_to_hal(spi);
  if (spi_h == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  HAL_StatusTypeDef status = HAL_SPI_Abort(spi_h);
  if (status == HAL_OK) {
    spi_h->ErrorCode = HAL_SPI_ERROR_NONE;
  }
  return platform_from_hal_status(status);
}

platform_status_t platform_spi_backend_abort_it(
  platform_spi_instance_t spi
) {
  SPI_HandleTypeDef *spi_h = platform_spi_to_hal(spi);
  return spi_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_Abort_IT(spi_h));
}

uint32_t platform_spi_backend_get_error(platform_spi_instance_t spi) {
  SPI_HandleTypeDef *spi_h = platform_spi_to_hal(spi);
  return spi_h == NULL ? PLATFORM_SPI_ERROR_UNKNOWN
                   : platform_spi_map_error(HAL_SPI_GetError(spi_h));
}

static void platform_spi_dispatch_hal_event(
  SPI_HandleTypeDef *spi_h,
  platform_spi_event_type_t type
) {
  platform_spi_instance_t spi;
  if (platform_hal_to_spi(spi_h, &spi)) {
    platform_spi_dispatch_event(spi, type);
  }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *spi_h) {
  platform_spi_dispatch_hal_event(spi_h, PLATFORM_SPI_EVENT_TX_COMPLETE);
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *spi_h) {
  platform_spi_dispatch_hal_event(spi_h, PLATFORM_SPI_EVENT_RX_COMPLETE);
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *spi_h) {
  platform_spi_dispatch_hal_event(spi_h, PLATFORM_SPI_EVENT_TX_RX_COMPLETE);
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *spi_h) {
  platform_spi_dispatch_hal_event(spi_h, PLATFORM_SPI_EVENT_ERROR);
}

void HAL_SPI_AbortCpltCallback(SPI_HandleTypeDef *spi_h) {
  platform_spi_dispatch_hal_event(spi_h, PLATFORM_SPI_EVENT_ABORT_COMPLETE);
}
