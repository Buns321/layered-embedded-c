/**
 * @file platform_spi_backend.h
 * @brief STM32 系列公共 SPI 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_SPI_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_SPI_BACKEND_H

#include "platform_def.h"
#include "platform_spi.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 校验SPI实例是否已由板级配置启用。
 */
bool platform_spi_backend_is_valid(platform_spi_instance_t spi);

/**
 * @brief 返回 SPI 后端当前数据宽度。
 */
platform_status_t platform_spi_backend_get_data_width(
  platform_spi_instance_t spi,
  platform_spi_data_width_t *data_width
);

/**
 * @brief 启动SPI阻塞发送。
 */
platform_status_t platform_spi_backend_transmit(
  platform_spi_instance_t spi,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief 启动SPI阻塞接收。
 */
platform_status_t platform_spi_backend_receive(
  platform_spi_instance_t spi,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief 启动 SPI 阻塞全双工传输。
 */
platform_status_t platform_spi_backend_transmit_receive(
  platform_spi_instance_t spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief 启动SPI中断发送。
 */
platform_status_t platform_spi_backend_transmit_it(
  platform_spi_instance_t spi,
  const uint8_t *data,
  uint16_t size
);

/**
 * @brief 启动SPI中断接收。
 */
platform_status_t platform_spi_backend_receive_it(
  platform_spi_instance_t spi,
  uint8_t *data,
  uint16_t size
);

/**
 * @brief 启动 SPI 中断全双工传输。
 */
platform_status_t platform_spi_backend_transmit_receive_it(
  platform_spi_instance_t spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
);

/**
 * @brief 启动SPI DMA 发送。
 */
platform_status_t platform_spi_backend_transmit_dma(
  platform_spi_instance_t spi,
  const uint8_t *data,
  uint16_t size
);

/**
 * @brief 启动SPI DMA 接收。
 */
platform_status_t platform_spi_backend_receive_dma(
  platform_spi_instance_t spi,
  uint8_t *data,
  uint16_t size
);

/**
 * @brief 启动 SPI DMA 全双工传输。
 */
platform_status_t platform_spi_backend_transmit_receive_dma(
  platform_spi_instance_t spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
);

/**
 * @brief 中止SPI当前传输并映射 HAL 状态。
 */
platform_status_t platform_spi_backend_abort(platform_spi_instance_t spi);

/**
 * @brief 启动SPI异步中止并映射 HAL 状态。
 */
platform_status_t platform_spi_backend_abort_it(
  platform_spi_instance_t spi
);

/**
 * @brief 返回SPI后端记录的错误位。
 */
uint32_t platform_spi_backend_get_error(platform_spi_instance_t spi);

/**
 * @brief 把 SPI 完成、错误和中止事件分发给已注册回调。
 */
void platform_spi_dispatch_event(
  platform_spi_instance_t spi,
  platform_spi_event_type_t type
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_SPI_BACKEND_H
