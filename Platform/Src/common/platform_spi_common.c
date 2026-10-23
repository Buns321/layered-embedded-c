/**
 * @file platform_spi_common.c
 * @brief Family-independent SPI validation, errors and callback dispatch.
 */

#include "platform_error_internal.h"
#include "platform_spi_backend.h"
#include "platform_validation_internal.h"
#include <stddef.h>

static volatile uint32_t spi_error[PLATFORM_SPI_INSTANCE_COUNT];
static platform_spi_callbacks_t spi_cb[PLATFORM_SPI_INSTANCE_COUNT];
static bool spi_cb_registered[PLATFORM_SPI_INSTANCE_COUNT];

static Platform_StatusTypeDef platform_spi_finish(
  Platform_SPI_Instance spi,
  Platform_StatusTypeDef status
) {
  if (status != Platform_OK) {
    platform_error_record(
      &spi_error[spi],
      platform_error_from_status(status)
    );
  }
  return status;
}

static Platform_StatusTypeDef platform_spi_invalid(Platform_SPI_Instance spi) {
  if ((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT) {
    platform_error_record(
      &spi_error[spi],
      PLATFORM_COMMON_ERROR_PARAMETER
    );
  }
  return Platform_INVALID_ARGUMENT;
}

static bool platform_spi_validate(
  Platform_SPI_Instance spi,
  const void *data,
  uint16_t size
) {
  if (
    (uint32_t)spi >= PLATFORM_SPI_INSTANCE_COUNT ||
    !platform_spi_backend_is_valid(spi)
  ) {
    return false;
  }

  Platform_SPI_DataWidth data_width;
  if (
    platform_spi_backend_get_data_width(spi, &data_width) != Platform_OK
  ) {
    return false;
  }
  return platform_spi_transfer_values_are_valid(
    data,
    size,
    data_width == PLATFORM_SPI_DATA_WIDTH_16_BIT
  );
}

void platform_spi_dispatch_event(
  Platform_SPI_Instance spi,
  Platform_SPI_EventType type
) {
  if (
    (uint32_t)spi >= PLATFORM_SPI_INSTANCE_COUNT ||
    !spi_cb_registered[spi]
  ) {
    return;
  }

  Platform_SPI_Event event = {
    .type = type,
    .error = type == PLATFORM_SPI_EVENT_ERROR
               ? platform_spi_backend_get_error(spi)
               : PLATFORM_SPI_ERROR_NONE,
  };
  if (spi_cb[spi].callback != NULL) {
    spi_cb[spi].callback(spi, &event, spi_cb[spi].context);
  }
}

Platform_StatusTypeDef Platform_SPI_GetDataWidth(
  Platform_SPI_Instance spi,
  Platform_SPI_DataWidth *data_width
) {
  if ((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT) {
    platform_error_reset(&spi_error[spi]);
  }
  if (
    (uint32_t)spi >= PLATFORM_SPI_INSTANCE_COUNT ||
    data_width == NULL ||
    !platform_spi_backend_is_valid(spi)
  ) {
    return platform_spi_invalid(spi);
  }
  return platform_spi_finish(
    spi,
    platform_spi_backend_get_data_width(spi, data_width)
  );
}

Platform_StatusTypeDef Platform_SPI_Transmit(
  Platform_SPI_Instance spi,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  if ((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT) {
    platform_error_reset(&spi_error[spi]);
  }
  if (!platform_spi_validate(spi, data, size)) {
    return platform_spi_invalid(spi);
  }
  return platform_spi_finish(
    spi,
    platform_spi_backend_transmit(spi, data, size, timeout)
  );
}

Platform_StatusTypeDef Platform_SPI_Receive(
  Platform_SPI_Instance spi,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  if ((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT) {
    platform_error_reset(&spi_error[spi]);
  }
  if (!platform_spi_validate(spi, data, size)) {
    return platform_spi_invalid(spi);
  }
  return platform_spi_finish(
    spi,
    platform_spi_backend_receive(spi, data, size, timeout)
  );
}

Platform_StatusTypeDef Platform_SPI_TransmitReceive(
  Platform_SPI_Instance spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size,
  uint32_t timeout
) {
  if ((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT) {
    platform_error_reset(&spi_error[spi]);
  }
  if (
    !platform_spi_validate(spi, tx_data, size) ||
    !platform_spi_validate(spi, rx_data, size)
  ) {
    return platform_spi_invalid(spi);
  }
  return platform_spi_finish(
    spi,
    platform_spi_backend_transmit_receive(
      spi,
      tx_data,
      rx_data,
      size,
      timeout
    )
  );
}

Platform_StatusTypeDef Platform_SPI_Transmit_IT(
  Platform_SPI_Instance spi,
  const uint8_t *data,
  uint16_t size
) {
  if ((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT) {
    platform_error_reset(&spi_error[spi]);
  }
  if (!platform_spi_validate(spi, data, size)) {
    return platform_spi_invalid(spi);
  }
  return platform_spi_finish(
    spi,
    platform_spi_backend_transmit_it(spi, data, size)
  );
}

Platform_StatusTypeDef Platform_SPI_Receive_IT(
  Platform_SPI_Instance spi,
  uint8_t *data,
  uint16_t size
) {
  if ((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT) {
    platform_error_reset(&spi_error[spi]);
  }
  if (!platform_spi_validate(spi, data, size)) {
    return platform_spi_invalid(spi);
  }
  return platform_spi_finish(
    spi,
    platform_spi_backend_receive_it(spi, data, size)
  );
}

Platform_StatusTypeDef Platform_SPI_TransmitReceive_IT(
  Platform_SPI_Instance spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
) {
  if ((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT) {
    platform_error_reset(&spi_error[spi]);
  }
  if (
    !platform_spi_validate(spi, tx_data, size) ||
    !platform_spi_validate(spi, rx_data, size)
  ) {
    return platform_spi_invalid(spi);
  }
  return platform_spi_finish(
    spi,
    platform_spi_backend_transmit_receive_it(
      spi,
      tx_data,
      rx_data,
      size
    )
  );
}

Platform_StatusTypeDef Platform_SPI_Transmit_DMA(
  Platform_SPI_Instance spi,
  const uint8_t *data,
  uint16_t size
) {
  if ((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT) {
    platform_error_reset(&spi_error[spi]);
  }
  if (!platform_spi_validate(spi, data, size)) {
    return platform_spi_invalid(spi);
  }
  return platform_spi_finish(
    spi,
    platform_spi_backend_transmit_dma(spi, data, size)
  );
}

Platform_StatusTypeDef Platform_SPI_Receive_DMA(
  Platform_SPI_Instance spi,
  uint8_t *data,
  uint16_t size
) {
  if ((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT) {
    platform_error_reset(&spi_error[spi]);
  }
  if (!platform_spi_validate(spi, data, size)) {
    return platform_spi_invalid(spi);
  }
  return platform_spi_finish(
    spi,
    platform_spi_backend_receive_dma(spi, data, size)
  );
}

Platform_StatusTypeDef Platform_SPI_TransmitReceive_DMA(
  Platform_SPI_Instance spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
) {
  if ((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT) {
    platform_error_reset(&spi_error[spi]);
  }
  if (
    !platform_spi_validate(spi, tx_data, size) ||
    !platform_spi_validate(spi, rx_data, size)
  ) {
    return platform_spi_invalid(spi);
  }
  return platform_spi_finish(
    spi,
    platform_spi_backend_transmit_receive_dma(
      spi,
      tx_data,
      rx_data,
      size
    )
  );
}

Platform_StatusTypeDef Platform_SPI_Abort(Platform_SPI_Instance spi) {
  if ((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT) {
    platform_error_reset(&spi_error[spi]);
  }
  if (!platform_spi_backend_is_valid(spi)) {
    return platform_spi_invalid(spi);
  }
  return platform_spi_finish(spi, platform_spi_backend_abort(spi));
}

Platform_StatusTypeDef Platform_SPI_Abort_IT(Platform_SPI_Instance spi) {
  if ((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT) {
    platform_error_reset(&spi_error[spi]);
  }
  if (!platform_spi_backend_is_valid(spi)) {
    return platform_spi_invalid(spi);
  }
  return platform_spi_finish(spi, platform_spi_backend_abort_it(spi));
}

uint32_t Platform_SPI_GetLastError(Platform_SPI_Instance spi) {
  if ((uint32_t)spi >= PLATFORM_SPI_INSTANCE_COUNT) {
    return PLATFORM_COMMON_ERROR_PARAMETER;
  }
  return spi_error[spi] | platform_spi_backend_get_error(spi);
}

Platform_StatusTypeDef Platform_SPI_RegisterCallbacks(
  Platform_SPI_Instance spi,
  const platform_spi_callbacks_t *callbacks
) {
  if (
    (uint32_t)spi >= PLATFORM_SPI_INSTANCE_COUNT ||
    callbacks == NULL ||
    !platform_spi_backend_is_valid(spi)
  ) {
    return Platform_INVALID_ARGUMENT;
  }
  if (spi_cb_registered[spi]) {
    return Platform_BUSY;
  }

  spi_cb[spi] = *callbacks;
  spi_cb_registered[spi] = true;
  return Platform_OK;
}
