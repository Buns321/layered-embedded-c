/**
 * @file platform_spi_common.c
 * @brief Family-independent SPI validation, errors and callback dispatch.
 */

#include "platform_error_internal.h"
#include "platform_spi_backend.h"
#include "platform_validation_internal.h"
#include <stddef.h>

static volatile uint32_t spi_error[PLATFORM_SPI_INSTANCE_COUNT];
static platform_spi_callback_binding_t spi_cb[PLATFORM_SPI_INSTANCE_COUNT];
static bool spi_cb_registered[PLATFORM_SPI_INSTANCE_COUNT];

static platform_status_t platform_spi_finish(
  platform_spi_instance_t spi,
  platform_status_t status
) {
  if (status != PLATFORM_STATUS_OK) {
    platform_error_record(
      &spi_error[spi],
      platform_error_from_status(status)
    );
  }
  return status;
}

static platform_status_t platform_spi_invalid(platform_spi_instance_t spi) {
  if ((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT) {
    platform_error_record(
      &spi_error[spi],
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
  }
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

static bool platform_spi_validate(
  platform_spi_instance_t spi,
  const void *data,
  uint16_t size
) {
  if (
    (uint32_t)spi >= PLATFORM_SPI_INSTANCE_COUNT ||
    !platform_spi_backend_is_valid(spi)
  ) {
    return false;
  }

  platform_spi_data_width_t data_width;
  if (
    platform_spi_backend_get_data_width(spi, &data_width) != PLATFORM_STATUS_OK
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
  platform_spi_instance_t spi,
  platform_spi_event_type_t type
) {
  if (
    (uint32_t)spi >= PLATFORM_SPI_INSTANCE_COUNT ||
    !spi_cb_registered[spi]
  ) {
    return;
  }

  platform_spi_event_t event = {
    .type = type,
    .error = type == PLATFORM_SPI_EVENT_ERROR
               ? platform_spi_backend_get_error(spi)
               : PLATFORM_SPI_ERROR_NONE,
  };
  if (spi_cb[spi].callback != NULL) {
    spi_cb[spi].callback(spi, &event, spi_cb[spi].context);
  }
}

platform_status_t Platform_SPI_GetDataWidth(
  platform_spi_instance_t spi,
  platform_spi_data_width_t *data_width
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

platform_status_t Platform_SPI_Transmit(
  platform_spi_instance_t spi,
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

platform_status_t Platform_SPI_Receive(
  platform_spi_instance_t spi,
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

platform_status_t Platform_SPI_TransmitReceive(
  platform_spi_instance_t spi,
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

platform_status_t Platform_SPI_TransmitIT(
  platform_spi_instance_t spi,
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

platform_status_t Platform_SPI_ReceiveIT(
  platform_spi_instance_t spi,
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

platform_status_t Platform_SPI_TransmitReceiveIT(
  platform_spi_instance_t spi,
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

platform_status_t Platform_SPI_TransmitDMA(
  platform_spi_instance_t spi,
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

platform_status_t Platform_SPI_ReceiveDMA(
  platform_spi_instance_t spi,
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

platform_status_t Platform_SPI_TransmitReceiveDMA(
  platform_spi_instance_t spi,
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

platform_status_t Platform_SPI_Abort(platform_spi_instance_t spi) {
  if ((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT) {
    platform_error_reset(&spi_error[spi]);
  }
  if (!platform_spi_backend_is_valid(spi)) {
    return platform_spi_invalid(spi);
  }
  return platform_spi_finish(spi, platform_spi_backend_abort(spi));
}

platform_status_t Platform_SPI_AbortIT(platform_spi_instance_t spi) {
  if ((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT) {
    platform_error_reset(&spi_error[spi]);
  }
  if (!platform_spi_backend_is_valid(spi)) {
    return platform_spi_invalid(spi);
  }
  return platform_spi_finish(spi, platform_spi_backend_abort_it(spi));
}

uint32_t Platform_SPI_GetLastError(platform_spi_instance_t spi) {
  if ((uint32_t)spi >= PLATFORM_SPI_INSTANCE_COUNT) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return spi_error[spi] | platform_spi_backend_get_error(spi);
}

platform_status_t Platform_SPI_RegisterCallback(
  platform_spi_instance_t spi,
  const platform_spi_callback_binding_t *callbacks
) {
  if (
    (uint32_t)spi >= PLATFORM_SPI_INSTANCE_COUNT ||
    callbacks == NULL ||
    !platform_spi_backend_is_valid(spi)
  ) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (spi_cb_registered[spi]) {
    return PLATFORM_STATUS_BUSY;
  }

  spi_cb[spi] = *callbacks;
  spi_cb_registered[spi] = true;
  return PLATFORM_STATUS_OK;
}
