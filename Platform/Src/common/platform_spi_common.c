/**
 * @file platform_spi_common.c
 * @brief 与芯片系列无关的 SPI 参数校验、错误管理和回调分发。
 * @ingroup platform
 */

#include "platform_error_internal.h"
#include "platform_spi_backend.h"
#include "platform_validation_internal.h"
#include <stddef.h>

static volatile uint32_t spi_error[PLATFORM_SPI_INSTANCE_COUNT];
static platform_spi_callback_binding_t spi_cb[PLATFORM_SPI_INSTANCE_COUNT];
static bool spi_cb_registered[PLATFORM_SPI_INSTANCE_COUNT];

/**
 * @brief 把 HAL SPI 结果转换为公共状态并记录错误。
 */
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

/**
 * @brief 构造 SPI 非法参数状态。
 */
static platform_status_t platform_spi_invalid(platform_spi_instance_t spi) {
  if ((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT) {
    platform_error_record(
      &spi_error[spi],
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
  }
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

/**
 * @brief 校验 SPI 实例、缓冲区和长度参数。
 */
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

/**
 * @brief 把 SPI 完成、错误和中止事件分发给已注册回调。
 */
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

/**
 * @brief 读取 CubeMX 配置的数据宽度。
 * @param spi SPI 实例。
 * @param data_width 输出数据宽度。
 * @note 即使工作在 16 位模式，传输函数中的长度仍以字节为单位。
 * @retval PLATFORM_STATUS_OK 查询成功。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、输出指针或 CubeMX 数据宽度非法。
 */
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

/**
 * @brief 阻塞式主机发送。
 * @param spi SPI 实例。
 * @param data 大小为 size 字节的缓冲区，调用期间只读。
 * @param size 字节数。16 位模式下必须为偶数。
 * @param timeout 超时时间（ms）或 PLATFORM_TIMEOUT_INFINITE。
 * @pre MX_SPIx_Init() 已完成，且调用方已拉低所需片选。
 */
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

/**
 * @brief 阻塞式主机接收。
 * @param spi SPI 实例。
 * @param data 接收缓冲区。
 * @param size 字节数。16 位模式下必须为偶数。
 * @param timeout 超时时间（ms）或 PLATFORM_TIMEOUT_INFINITE。
 */
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

/**
 * @brief 阻塞式全双工主机传输。
 * @param spi SPI 实例。
 * @param tx_data 发送缓冲区。
 * @param rx_data 接收缓冲区。
 * @param size 字节数。16 位模式下必须为偶数。
 * @param timeout 超时时间（ms）或 PLATFORM_TIMEOUT_INFINITE。
 */
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

/**
 * @brief 中断方式主机发送。
 * @param spi SPI 实例。
 * @param data 发送缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，缓冲区必须保持有效。
 */
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

/**
 * @brief 中断方式主机接收。
 * @param spi SPI 实例。
 * @param data 接收缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，缓冲区必须保持有效。
 */
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

/**
 * @brief 中断方式全双工主机传输。
 * @param spi SPI 实例。
 * @param tx_data 发送缓冲区。
 * @param rx_data 接收缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，两个缓冲区都必须保持有效。
 */
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

/**
 * @brief DMA 方式主机发送。
 * @param spi SPI 实例。
 * @param data 发送缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，缓冲区必须保持有效。
 */
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

/**
 * @brief DMA 方式主机接收。
 * @param spi SPI 实例。
 * @param data 接收缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，缓冲区必须保持有效。
 */
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

/**
 * @brief DMA 方式全双工主机传输。
 * @param spi SPI 实例。
 * @param tx_data 发送缓冲区。
 * @param rx_data 接收缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，两个缓冲区都必须保持有效。
 */
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

/**
 * @brief 同步中止当前传输并清除错误快照。
 * @param spi SPI 实例。
 * @note 只能从任务上下文调用，禁止在 ISR 中调用。
 */
platform_status_t Platform_SPI_Abort(platform_spi_instance_t spi) {
  if ((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT) {
    platform_error_reset(&spi_error[spi]);
  }
  if (!platform_spi_backend_is_valid(spi)) {
    return platform_spi_invalid(spi);
  }
  return platform_spi_finish(spi, platform_spi_backend_abort(spi));
}

/**
 * @brief 使用中断方式中止当前传输。
 * @param spi SPI 实例。
 * @note 中止完成回调在中断上下文执行。
 */
platform_status_t Platform_SPI_AbortIT(platform_spi_instance_t spi) {
  if ((uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT) {
    platform_error_reset(&spi_error[spi]);
  }
  if (!platform_spi_backend_is_valid(spi)) {
    return platform_spi_invalid(spi);
  }
  return platform_spi_finish(spi, platform_spi_backend_abort_it(spi));
}

/**
 * @brief 读取最近一次 HAL SPI 错误快照，并转换为 platform_spi_error_t 位。
 * @param spi SPI 实例。
 */
uint32_t Platform_SPI_GetLastError(platform_spi_instance_t spi) {
  if ((uint32_t)spi >= PLATFORM_SPI_INSTANCE_COUNT) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return spi_error[spi] | platform_spi_backend_get_error(spi);
}

/**
 * @brief 注册 SPI 回调。
 * @param spi SPI 实例。
 * @param callbacks 待复制的回调集合。
 * @note 结构体会被复制。应在初始化阶段、启动 IT/DMA 传输前调用一次。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该实例已注册回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或回调集合非法。
 */
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
