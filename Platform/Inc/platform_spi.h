//
// 由 Buns_ 创建于 2026/10/23。
//

/**
 * @file platform_spi.h
 * @brief 不依赖 HAL 类型的 SPI 主机传输接口。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_SPI_H
#define LAYERED_EMBEDDED_C_PLATFORM_SPI_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

#if !PLATFORM_ENABLE_SPI1 && !PLATFORM_ENABLE_SPI2 && \
    !PLATFORM_ENABLE_SPI3
#error "At least one Platform SPI instance must be enabled"
#endif

/** @brief 可用的 SPI 实例。 */
typedef enum platform_spi_instance_e {
#if PLATFORM_ENABLE_SPI1
  PLATFORM_SPI_1 = 0U,
#endif
#if PLATFORM_ENABLE_SPI2
  PLATFORM_SPI_2 = 1U,
#endif
#if PLATFORM_ENABLE_SPI3
  PLATFORM_SPI_3 = 2U,
#endif
} platform_spi_instance_t;

/** @brief SPI 身份槽位数量上限。 */
#define PLATFORM_SPI_INSTANCE_COUNT 3U

/** @brief CubeMX 为 SPI 实例配置的数据宽度。 */
typedef enum platform_spi_data_width_e {
  PLATFORM_SPI_DATA_WIDTH_8_BIT = 8U,
  PLATFORM_SPI_DATA_WIDTH_16_BIT = 16U,
} platform_spi_data_width_t;

/** @brief SPI 错误标志，可同时置位多个位。 */
typedef enum platform_spi_error_e {
  PLATFORM_SPI_ERROR_NONE    = 0u,
  PLATFORM_SPI_ERROR_MODF    = (1u << 0),
  PLATFORM_SPI_ERROR_CRC     = (1u << 1),
  PLATFORM_SPI_ERROR_OVR     = (1u << 2),
  PLATFORM_SPI_ERROR_FRE     = (1u << 3),
  PLATFORM_SPI_ERROR_DMA     = (1u << 4),
  PLATFORM_SPI_ERROR_FLAG    = (1u << 5),
  PLATFORM_SPI_ERROR_ABORT   = (1u << 6),
  PLATFORM_SPI_ERROR_UNKNOWN = (1u << 7),
} platform_spi_error_t;

/** @brief SPI 异步事件。 */
typedef enum platform_spi_event_type_e {
  PLATFORM_SPI_EVENT_TX_COMPLETE = 0U,
  PLATFORM_SPI_EVENT_RX_COMPLETE,
  PLATFORM_SPI_EVENT_TX_RX_COMPLETE,
  PLATFORM_SPI_EVENT_ERROR,
  PLATFORM_SPI_EVENT_ABORT_COMPLETE,
} platform_spi_event_type_t;

/** @brief SPI 事件数据，只在回调期间有效。 */
typedef struct platform_spi_event_s {
  platform_spi_event_type_t type;
  uint32_t error;
} platform_spi_event_t;

typedef void (*platform_spi_callback_t)(
  platform_spi_instance_t spi,
  const platform_spi_event_t *event,
  void *context
);

/**
 * @brief SPI 回调集合。
 * @note 回调在中断上下文执行。
 * @note 一个物理实例在整个生命周期内只能有一个回调所有者。
 */
typedef struct platform_spi_callback_binding_s {
  platform_spi_callback_t callback;
  void *context;
} platform_spi_callback_binding_t;

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
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

/**
 * @brief 同步中止当前传输并清除错误快照。
 * @param spi SPI 实例。
 * @note 只能从任务上下文调用，禁止在 ISR 中调用。
 */
platform_status_t Platform_SPI_Abort(platform_spi_instance_t spi);

/**
 * @brief 使用中断方式中止当前传输。
 * @param spi SPI 实例。
 * @note 中止完成回调在中断上下文执行。
 */
platform_status_t Platform_SPI_AbortIT(platform_spi_instance_t spi);

/**
 * @brief 读取最近一次 HAL SPI 错误快照，并转换为 platform_spi_error_t 位。
 * @param spi SPI 实例。
 */
uint32_t Platform_SPI_GetLastError(platform_spi_instance_t spi);

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
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_SPI_H
