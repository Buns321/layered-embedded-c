#include "platform_spi_backend.h"
#include "platform_spi.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static Platform_SPI_DataWidth s_data_width;
static Platform_StatusTypeDef s_transmit_status;
static uint32_t s_backend_error;
static uint32_t s_event_count;

bool platform_spi_backend_is_valid(Platform_SPI_Instance spi) {
  return spi == PLATFORM_SPI_1;
}

Platform_StatusTypeDef platform_spi_backend_get_data_width(
  Platform_SPI_Instance spi,
  Platform_SPI_DataWidth *data_width
) {
  (void)spi;
  *data_width = s_data_width;
  return Platform_OK;
}

Platform_StatusTypeDef platform_spi_backend_transmit(
  Platform_SPI_Instance spi,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)spi;
  (void)data;
  (void)size;
  (void)timeout;
  return s_transmit_status;
}

Platform_StatusTypeDef platform_spi_backend_receive(
  Platform_SPI_Instance spi,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)spi;
  (void)data;
  (void)size;
  (void)timeout;
  return Platform_OK;
}

Platform_StatusTypeDef platform_spi_backend_transmit_receive(
  Platform_SPI_Instance spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size,
  uint32_t timeout
) {
  (void)spi;
  (void)tx_data;
  (void)rx_data;
  (void)size;
  (void)timeout;
  return Platform_OK;
}

Platform_StatusTypeDef platform_spi_backend_transmit_it(
  Platform_SPI_Instance spi,
  const uint8_t *data,
  uint16_t size
) {
  (void)spi;
  (void)data;
  (void)size;
  return Platform_OK;
}

Platform_StatusTypeDef platform_spi_backend_receive_it(
  Platform_SPI_Instance spi,
  uint8_t *data,
  uint16_t size
) {
  (void)spi;
  (void)data;
  (void)size;
  return Platform_OK;
}

Platform_StatusTypeDef platform_spi_backend_transmit_receive_it(
  Platform_SPI_Instance spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
) {
  (void)spi;
  (void)tx_data;
  (void)rx_data;
  (void)size;
  return Platform_OK;
}

Platform_StatusTypeDef platform_spi_backend_transmit_dma(
  Platform_SPI_Instance spi,
  const uint8_t *data,
  uint16_t size
) {
  (void)spi;
  (void)data;
  (void)size;
  return Platform_OK;
}

Platform_StatusTypeDef platform_spi_backend_receive_dma(
  Platform_SPI_Instance spi,
  uint8_t *data,
  uint16_t size
) {
  (void)spi;
  (void)data;
  (void)size;
  return Platform_OK;
}

Platform_StatusTypeDef platform_spi_backend_transmit_receive_dma(
  Platform_SPI_Instance spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
) {
  (void)spi;
  (void)tx_data;
  (void)rx_data;
  (void)size;
  return Platform_OK;
}

Platform_StatusTypeDef platform_spi_backend_abort(Platform_SPI_Instance spi) {
  (void)spi;
  return Platform_OK;
}

Platform_StatusTypeDef platform_spi_backend_abort_it(
  Platform_SPI_Instance spi
) {
  (void)spi;
  return Platform_OK;
}

uint32_t platform_spi_backend_get_error(Platform_SPI_Instance spi) {
  (void)spi;
  return s_backend_error;
}

static void spi_callback(
  Platform_SPI_Instance spi,
  const Platform_SPI_Event *event,
  void *context
) {
  assert(spi == PLATFORM_SPI_1);
  assert(event != NULL);
  assert(context == &s_event_count);
  s_event_count++;
}

static void test_spi_contract(void) {
  const uint8_t buffer[4] = {0U, 1U, 2U, 3U};
  s_transmit_status = Platform_OK;
  s_backend_error = PLATFORM_SPI_ERROR_NONE;
  s_event_count = 0U;

  Platform_SPI_DataWidth width;
  s_data_width = PLATFORM_SPI_DATA_WIDTH_16_BIT;
  assert(Platform_SPI_GetDataWidth(PLATFORM_SPI_1, &width) == Platform_OK);
  assert(width == PLATFORM_SPI_DATA_WIDTH_16_BIT);
  assert(Platform_SPI_Transmit(PLATFORM_SPI_1, buffer, 3U, 10U) ==
         Platform_INVALID_ARGUMENT);
  assert((Platform_SPI_GetLastError(PLATFORM_SPI_1) &
          PLATFORM_COMMON_ERROR_PARAMETER) != 0U);

  s_data_width = PLATFORM_SPI_DATA_WIDTH_8_BIT;
  s_transmit_status = Platform_ERROR;
  assert(Platform_SPI_Transmit(PLATFORM_SPI_1, buffer, 3U, 10U) ==
         Platform_ERROR);
  assert((Platform_SPI_GetLastError(PLATFORM_SPI_1) &
          PLATFORM_COMMON_ERROR_HAL) != 0U);

  s_transmit_status = Platform_OK;
  assert(Platform_SPI_Transmit(PLATFORM_SPI_1, buffer, 3U, 10U) ==
         Platform_OK);

  const platform_spi_callbacks_t callbacks = {
    .callback = spi_callback,
    .context = &s_event_count,
  };
  assert(Platform_SPI_RegisterCallbacks(PLATFORM_SPI_1, &callbacks) ==
         Platform_OK);
  assert(Platform_SPI_RegisterCallbacks(PLATFORM_SPI_1, &callbacks) ==
         Platform_BUSY);

  s_backend_error = PLATFORM_SPI_ERROR_OVR;
  platform_spi_dispatch_event(PLATFORM_SPI_1, PLATFORM_SPI_EVENT_ERROR);
  assert(s_event_count == 1U);
}

int main(void) {
  test_spi_contract();
  return 0;
}
