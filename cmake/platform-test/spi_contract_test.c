/**
 * @file spi_contract_test.c
 * @brief SPI Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_spi_backend.h"
#include "platform_spi.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static platform_spi_data_width_t s_data_width;
static platform_status_t s_transmit_status;
static uint32_t s_backend_error;
static uint32_t s_event_count;

/**
 * @brief 校验SPI实例是否已由板级配置启用。
 */
bool platform_spi_backend_is_valid(platform_spi_instance_t spi) {
  return spi == PLATFORM_SPI_1;
}

/**
 * @brief 返回 SPI 后端当前数据宽度。
 */
platform_status_t platform_spi_backend_get_data_width(
  platform_spi_instance_t spi,
  platform_spi_data_width_t *data_width
) {
  (void)spi;
  *data_width = s_data_width;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 启动SPI阻塞发送。
 */
platform_status_t platform_spi_backend_transmit(
  platform_spi_instance_t spi,
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

/**
 * @brief 启动SPI阻塞接收。
 */
platform_status_t platform_spi_backend_receive(
  platform_spi_instance_t spi,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)spi;
  (void)data;
  (void)size;
  (void)timeout;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 启动 SPI 阻塞全双工传输。
 */
platform_status_t platform_spi_backend_transmit_receive(
  platform_spi_instance_t spi,
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
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 启动SPI中断发送。
 */
platform_status_t platform_spi_backend_transmit_it(
  platform_spi_instance_t spi,
  const uint8_t *data,
  uint16_t size
) {
  (void)spi;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 启动SPI中断接收。
 */
platform_status_t platform_spi_backend_receive_it(
  platform_spi_instance_t spi,
  uint8_t *data,
  uint16_t size
) {
  (void)spi;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 启动 SPI 中断全双工传输。
 */
platform_status_t platform_spi_backend_transmit_receive_it(
  platform_spi_instance_t spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
) {
  (void)spi;
  (void)tx_data;
  (void)rx_data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 启动SPI DMA 发送。
 */
platform_status_t platform_spi_backend_transmit_dma(
  platform_spi_instance_t spi,
  const uint8_t *data,
  uint16_t size
) {
  (void)spi;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 启动SPI DMA 接收。
 */
platform_status_t platform_spi_backend_receive_dma(
  platform_spi_instance_t spi,
  uint8_t *data,
  uint16_t size
) {
  (void)spi;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 启动 SPI DMA 全双工传输。
 */
platform_status_t platform_spi_backend_transmit_receive_dma(
  platform_spi_instance_t spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
) {
  (void)spi;
  (void)tx_data;
  (void)rx_data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 中止SPI当前传输并映射 HAL 状态。
 */
platform_status_t platform_spi_backend_abort(platform_spi_instance_t spi) {
  (void)spi;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 启动SPI异步中止并映射 HAL 状态。
 */
platform_status_t platform_spi_backend_abort_it(
  platform_spi_instance_t spi
) {
  (void)spi;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 返回SPI后端记录的错误位。
 */
uint32_t platform_spi_backend_get_error(platform_spi_instance_t spi) {
  (void)spi;
  return s_backend_error;
}

/**
 * @brief 记录 SPI 完成或错误事件。
 */
static void spi_callback(
  platform_spi_instance_t spi,
  const platform_spi_event_t *event,
  void *context
) {
  assert(spi == PLATFORM_SPI_1);
  assert(event != NULL);
  assert(context == &s_event_count);
  s_event_count++;
}

/**
 * @brief 验证 SPI 参数校验、收发和事件分发。
 */
static void test_spi_contract(void) {
  const uint8_t buffer[4] = {0U, 1U, 2U, 3U};
  s_transmit_status = PLATFORM_STATUS_OK;
  s_backend_error = PLATFORM_SPI_ERROR_NONE;
  s_event_count = 0U;

  platform_spi_data_width_t width;
  s_data_width = PLATFORM_SPI_DATA_WIDTH_16_BIT;
  assert(Platform_SPI_GetDataWidth(PLATFORM_SPI_1, &width) == PLATFORM_STATUS_OK);
  assert(width == PLATFORM_SPI_DATA_WIDTH_16_BIT);
  assert(Platform_SPI_Transmit(PLATFORM_SPI_1, buffer, 3U, 10U) ==
         PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_SPI_GetLastError(PLATFORM_SPI_1) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);

  s_data_width = PLATFORM_SPI_DATA_WIDTH_8_BIT;
  s_transmit_status = PLATFORM_STATUS_ERROR;
  assert(Platform_SPI_Transmit(PLATFORM_SPI_1, buffer, 3U, 10U) ==
         PLATFORM_STATUS_ERROR);
  assert((Platform_SPI_GetLastError(PLATFORM_SPI_1) &
          PLATFORM_ERROR_HAL) != 0U);

  s_transmit_status = PLATFORM_STATUS_OK;
  assert(Platform_SPI_Transmit(PLATFORM_SPI_1, buffer, 3U, 10U) ==
         PLATFORM_STATUS_OK);

  const platform_spi_callback_binding_t callbacks = {
    .callback = spi_callback,
    .context = &s_event_count,
  };
  assert(Platform_SPI_RegisterCallback(PLATFORM_SPI_1, &callbacks) ==
         PLATFORM_STATUS_OK);
  assert(Platform_SPI_RegisterCallback(PLATFORM_SPI_1, &callbacks) ==
         PLATFORM_STATUS_BUSY);

  s_backend_error = PLATFORM_SPI_ERROR_OVR;
  platform_spi_dispatch_event(PLATFORM_SPI_1, PLATFORM_SPI_EVENT_ERROR);
  assert(s_event_count == 1U);
}

/**
 * @brief 运行本文件定义的契约测试并返回进程状态。
 */
int main(void) {
  test_spi_contract();
  return 0;
}
