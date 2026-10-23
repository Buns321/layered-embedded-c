//
// Created by Buns_ on 2026/10/23.
//

#include "platform_spi.h"
#include "platform_hal_internal.h"
#include "spi.h"
#include "stm32f1xx_hal.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static platform_spi_callbacks_t spi_cb[PLATFORM_SPI_INSTANCE_COUNT];
static bool spi_cb_registered[PLATFORM_SPI_INSTANCE_COUNT];

static SPI_HandleTypeDef *platform_spi_to_hal(Platform_SPI_Instance spi) {
  switch (spi) {
#if PLATFORM_ENABLE_SPI1
    case PLATFORM_SPI_1: return &hspi1;
#endif
#if PLATFORM_ENABLE_SPI2
    case PLATFORM_SPI_2: return &hspi2;
#endif
    default: assert(0); return NULL;
  }
}

static bool platform_hal_to_spi(
  SPI_HandleTypeDef *hspi,
  Platform_SPI_Instance *spi
) {
  assert(hspi != NULL && spi != NULL);

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

static void platform_spi_validate(
  Platform_SPI_Instance spi,
  const void *data,
  uint16_t size
) {
  assert(
    (uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT &&
    data != NULL &&
    size != 0U
  );
  (void)data;
  (void)size;

  SPI_HandleTypeDef *hspi = platform_spi_to_hal(spi);
  assert(hspi != NULL);

  if (hspi->Init.DataSize == SPI_DATASIZE_16BIT) {
    assert((size & 1U) == 0U);
    assert(((uintptr_t)data & 1U) == 0U);
  } else {
    assert(hspi->Init.DataSize == SPI_DATASIZE_8BIT);
  }
}

static uint16_t platform_spi_hal_size(
  SPI_HandleTypeDef *hspi,
  uint16_t size
) {
  assert(hspi != NULL);
  if (hspi->Init.DataSize == SPI_DATASIZE_16BIT) {
    return (uint16_t)(size / 2U);
  }
  return size;
}

Platform_SPI_DataWidth Platform_SPI_GetDataWidth(
  Platform_SPI_Instance spi
) {
  assert((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT);
  SPI_HandleTypeDef *hspi = platform_spi_to_hal(spi);
  assert(hspi != NULL);

  if (hspi->Init.DataSize == SPI_DATASIZE_16BIT) {
    return PLATFORM_SPI_DATA_WIDTH_16_BIT;
  }
  assert(hspi->Init.DataSize == SPI_DATASIZE_8BIT);
  return PLATFORM_SPI_DATA_WIDTH_8_BIT;
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

Platform_StatusTypeDef Platform_SPI_Transmit(
  Platform_SPI_Instance spi,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  platform_spi_validate(spi, data, size);
  SPI_HandleTypeDef *hspi = platform_spi_to_hal(spi);
  return platform_from_hal_status(HAL_SPI_Transmit(
    hspi,
    (uint8_t *)data,
    platform_spi_hal_size(hspi, size),
    timeout
  ));
}

Platform_StatusTypeDef Platform_SPI_Receive(
  Platform_SPI_Instance spi,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  platform_spi_validate(spi, data, size);
  SPI_HandleTypeDef *hspi = platform_spi_to_hal(spi);
  return platform_from_hal_status(HAL_SPI_Receive(
    hspi,
    data,
    platform_spi_hal_size(hspi, size),
    timeout
  ));
}

Platform_StatusTypeDef Platform_SPI_TransmitReceive(
  Platform_SPI_Instance spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size,
  uint32_t timeout
) {
  platform_spi_validate(spi, tx_data, size);
  platform_spi_validate(spi, rx_data, size);
  SPI_HandleTypeDef *hspi = platform_spi_to_hal(spi);
  return platform_from_hal_status(HAL_SPI_TransmitReceive(
    hspi,
    (uint8_t *)tx_data,
    rx_data,
    platform_spi_hal_size(hspi, size),
    timeout
  ));
}

Platform_StatusTypeDef Platform_SPI_Transmit_IT(
  Platform_SPI_Instance spi,
  const uint8_t *data,
  uint16_t size
) {
  platform_spi_validate(spi, data, size);
  SPI_HandleTypeDef *hspi = platform_spi_to_hal(spi);
  return platform_from_hal_status(HAL_SPI_Transmit_IT(
    hspi,
    (uint8_t *)data,
    platform_spi_hal_size(hspi, size)
  ));
}

Platform_StatusTypeDef Platform_SPI_Receive_IT(
  Platform_SPI_Instance spi,
  uint8_t *data,
  uint16_t size
) {
  platform_spi_validate(spi, data, size);
  SPI_HandleTypeDef *hspi = platform_spi_to_hal(spi);
  return platform_from_hal_status(HAL_SPI_Receive_IT(
    hspi,
    data,
    platform_spi_hal_size(hspi, size)
  ));
}

Platform_StatusTypeDef Platform_SPI_TransmitReceive_IT(
  Platform_SPI_Instance spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
) {
  platform_spi_validate(spi, tx_data, size);
  platform_spi_validate(spi, rx_data, size);
  SPI_HandleTypeDef *hspi = platform_spi_to_hal(spi);
  return platform_from_hal_status(HAL_SPI_TransmitReceive_IT(
    hspi,
    (uint8_t *)tx_data,
    rx_data,
    platform_spi_hal_size(hspi, size)
  ));
}

Platform_StatusTypeDef Platform_SPI_Transmit_DMA(
  Platform_SPI_Instance spi,
  const uint8_t *data,
  uint16_t size
) {
  platform_spi_validate(spi, data, size);
  SPI_HandleTypeDef *hspi = platform_spi_to_hal(spi);
  return platform_from_hal_status(HAL_SPI_Transmit_DMA(
    hspi,
    (uint8_t *)data,
    platform_spi_hal_size(hspi, size)
  ));
}

Platform_StatusTypeDef Platform_SPI_Receive_DMA(
  Platform_SPI_Instance spi,
  uint8_t *data,
  uint16_t size
) {
  platform_spi_validate(spi, data, size);
  SPI_HandleTypeDef *hspi = platform_spi_to_hal(spi);
  return platform_from_hal_status(HAL_SPI_Receive_DMA(
    hspi,
    data,
    platform_spi_hal_size(hspi, size)
  ));
}

Platform_StatusTypeDef Platform_SPI_TransmitReceive_DMA(
  Platform_SPI_Instance spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
) {
  platform_spi_validate(spi, tx_data, size);
  platform_spi_validate(spi, rx_data, size);
  SPI_HandleTypeDef *hspi = platform_spi_to_hal(spi);
  return platform_from_hal_status(HAL_SPI_TransmitReceive_DMA(
    hspi,
    (uint8_t *)tx_data,
    rx_data,
    platform_spi_hal_size(hspi, size)
  ));
}

Platform_StatusTypeDef Platform_SPI_Abort(Platform_SPI_Instance spi) {
  assert((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT);
  SPI_HandleTypeDef *hspi = platform_spi_to_hal(spi);
  assert(hspi != NULL);

  HAL_StatusTypeDef status = HAL_SPI_Abort(hspi);
  if (status == HAL_OK) {
    hspi->ErrorCode = HAL_SPI_ERROR_NONE;
  }
  return platform_from_hal_status(status);
}

Platform_StatusTypeDef Platform_SPI_Abort_IT(Platform_SPI_Instance spi) {
  assert((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT);
  SPI_HandleTypeDef *hspi = platform_spi_to_hal(spi);
  assert(hspi != NULL);
  return platform_from_hal_status(HAL_SPI_Abort_IT(hspi));
}

uint32_t Platform_SPI_GetLastError(Platform_SPI_Instance spi) {
  assert((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT);
  SPI_HandleTypeDef *hspi = platform_spi_to_hal(spi);
  assert(hspi != NULL);
  return platform_spi_map_error(HAL_SPI_GetError(hspi));
}

void Platform_SPI_RegisterCallbacks(
  Platform_SPI_Instance spi,
  const platform_spi_callbacks_t *callbacks
) {
  assert(
    (uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT &&
    callbacks != NULL &&
    !spi_cb_registered[spi] &&
    platform_spi_to_hal(spi) != NULL
  );
  spi_cb[spi] = *callbacks;
  spi_cb_registered[spi] = true;
}

typedef enum {
  PLATFORM_SPI_EVENT_TX_CPLT = 0,
  PLATFORM_SPI_EVENT_RX_CPLT,
  PLATFORM_SPI_EVENT_TXRX_CPLT,
  PLATFORM_SPI_EVENT_ERROR,
  PLATFORM_SPI_EVENT_ABORT_CPLT,
} platform_spi_event_t;

static void platform_spi_dispatch(
  SPI_HandleTypeDef *hspi,
  platform_spi_event_t event
) {
  Platform_SPI_Instance spi;
  if (!platform_hal_to_spi(hspi, &spi)) {
    return;
  }

  Platform_SPI_Callback callback = NULL;
  switch (event) {
    case PLATFORM_SPI_EVENT_TX_CPLT:
      callback = spi_cb[spi].tx_cplt_cb;
      break;
    case PLATFORM_SPI_EVENT_RX_CPLT:
      callback = spi_cb[spi].rx_cplt_cb;
      break;
    case PLATFORM_SPI_EVENT_TXRX_CPLT:
      callback = spi_cb[spi].txrx_cplt_cb;
      break;
    case PLATFORM_SPI_EVENT_ERROR:
      callback = spi_cb[spi].err_cb;
      break;
    case PLATFORM_SPI_EVENT_ABORT_CPLT:
      callback = spi_cb[spi].abort_cplt_cb;
      break;
    default:
      assert(0);
      break;
  }

  if (callback != NULL) {
    callback(spi, spi_cb[spi].context);
  }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
  platform_spi_dispatch(hspi, PLATFORM_SPI_EVENT_TX_CPLT);
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
  platform_spi_dispatch(hspi, PLATFORM_SPI_EVENT_RX_CPLT);
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
  platform_spi_dispatch(hspi, PLATFORM_SPI_EVENT_TXRX_CPLT);
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
  platform_spi_dispatch(hspi, PLATFORM_SPI_EVENT_ERROR);
}

void HAL_SPI_AbortCpltCallback(SPI_HandleTypeDef *hspi) {
  platform_spi_dispatch(hspi, PLATFORM_SPI_EVENT_ABORT_CPLT);
}
