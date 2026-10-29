/**
 * @file platform_spi.c
 * @brief STM32F4 公共 SPI 实现的 HAL 后端。
 * @ingroup platform
 */

#include "platform_hal_internal.h"
#include "platform_spi_backend.h"
#include "spi.h"
#include "stm32f4xx_hal.h"

/**
 * @brief 把平台 SPI 实例编号转换为 HAL 句柄。
 */
static SPI_HandleTypeDef *platform_spi_to_hal(platform_spi_instance_t spi) {
  switch (spi) {
#if PLATFORM_ENABLE_SPI1
    case PLATFORM_SPI_1: return &hspi1;
#endif
#if PLATFORM_ENABLE_SPI2
    case PLATFORM_SPI_2: return &hspi2;
#endif
#if PLATFORM_ENABLE_SPI3
    case PLATFORM_SPI_3: return &hspi3;
#endif
    default: return NULL;
  }
}

/**
 * @brief 把 HAL SPI 句柄识别为平台实例编号。
 */
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
#if PLATFORM_ENABLE_SPI3
  if (spi_h == &hspi3) {
    *spi = PLATFORM_SPI_3;
    return true;
  }
#endif
  return false;
}

/**
 * @brief 把 SPI 数据宽度枚举转换为 HAL 位宽。
 */
static uint16_t platform_spi_hal_size(
  SPI_HandleTypeDef *spi_h,
  uint16_t size
) {
  return spi_h->Init.DataSize == SPI_DATASIZE_16BIT
           ? (uint16_t)(size / 2U)
           : size;
}

/**
 * @brief 把 HAL SPI 错误位映射为平台错误码。
 */
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
#ifdef HAL_SPI_ERROR_FRE
  if ((hal_error & HAL_SPI_ERROR_FRE) != 0U) {
    error |= PLATFORM_SPI_ERROR_FRE;
  }
#endif
  if ((hal_error & HAL_SPI_ERROR_DMA) != 0U) {
    error |= PLATFORM_SPI_ERROR_DMA;
  }
  if ((hal_error & HAL_SPI_ERROR_FLAG) != 0U) {
    error |= PLATFORM_SPI_ERROR_FLAG;
  }
  if ((hal_error & HAL_SPI_ERROR_ABORT) != 0U) {
    error |= PLATFORM_SPI_ERROR_ABORT;
  }

  uint32_t known_errors =
    HAL_SPI_ERROR_MODF |
    HAL_SPI_ERROR_CRC |
    HAL_SPI_ERROR_OVR |
    HAL_SPI_ERROR_DMA |
    HAL_SPI_ERROR_FLAG |
    HAL_SPI_ERROR_ABORT;
#ifdef HAL_SPI_ERROR_FRE
  known_errors |= HAL_SPI_ERROR_FRE;
#endif
  if ((hal_error & ~known_errors) != 0U) {
    error |= PLATFORM_SPI_ERROR_UNKNOWN;
  }
  return error;
}

/**
 * @brief 校验SPI实例是否已由板级配置启用。
 */
bool platform_spi_backend_is_valid(platform_spi_instance_t spi) {
  return (uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT &&
         platform_spi_to_hal(spi) != NULL;
}

/**
 * @brief 返回 SPI 后端当前数据宽度。
 */
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

/**
 * @brief 启动SPI阻塞发送。
 */
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

/**
 * @brief 启动SPI阻塞接收。
 */
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

/**
 * @brief 启动SPI中断发送。
 */
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

/**
 * @brief 启动SPI中断接收。
 */
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

/**
 * @brief 启动 SPI 中断全双工传输。
 */
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

/**
 * @brief 启动SPI DMA 发送。
 */
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

/**
 * @brief 启动SPI DMA 接收。
 */
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

/**
 * @brief 启动 SPI DMA 全双工传输。
 */
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

/**
 * @brief 中止SPI当前传输并映射 HAL 状态。
 */
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

/**
 * @brief 启动SPI异步中止并映射 HAL 状态。
 */
platform_status_t platform_spi_backend_abort_it(
  platform_spi_instance_t spi
) {
  SPI_HandleTypeDef *spi_h = platform_spi_to_hal(spi);
  return spi_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_Abort_IT(spi_h));
}

/**
 * @brief 返回SPI后端记录的错误位。
 */
uint32_t platform_spi_backend_get_error(platform_spi_instance_t spi) {
  SPI_HandleTypeDef *spi_h = platform_spi_to_hal(spi);
  return spi_h == NULL ? PLATFORM_SPI_ERROR_UNKNOWN
                   : platform_spi_map_error(HAL_SPI_GetError(spi_h));
}

/**
 * @brief 把 HAL SPI 回调事件转发给已注册的项目回调。
 */
static void platform_spi_dispatch_hal_event(
  SPI_HandleTypeDef *spi_h,
  platform_spi_event_type_t type
) {
  platform_spi_instance_t spi;
  if (platform_hal_to_spi(spi_h, &spi)) {
    platform_spi_dispatch_event(spi, type);
  }
}

/**
 * @brief 分发 SPI 发送完成事件。
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *spi_h) {
  platform_spi_dispatch_hal_event(spi_h, PLATFORM_SPI_EVENT_TX_COMPLETE);
}

/**
 * @brief 分发 SPI 接收完成事件。
 */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *spi_h) {
  platform_spi_dispatch_hal_event(spi_h, PLATFORM_SPI_EVENT_RX_COMPLETE);
}

/**
 * @brief 分发 SPI 全双工完成事件。
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *spi_h) {
  platform_spi_dispatch_hal_event(spi_h, PLATFORM_SPI_EVENT_TX_RX_COMPLETE);
}

/**
 * @brief 记录并分发 SPI 错误事件。
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *spi_h) {
  platform_spi_dispatch_hal_event(spi_h, PLATFORM_SPI_EVENT_ERROR);
}

/**
 * @brief 分发 SPI 中止完成事件。
 */
void HAL_SPI_AbortCpltCallback(SPI_HandleTypeDef *spi_h) {
  platform_spi_dispatch_hal_event(spi_h, PLATFORM_SPI_EVENT_ABORT_COMPLETE);
}
