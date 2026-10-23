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
typedef enum {
#if PLATFORM_ENABLE_SPI1
  PLATFORM_SPI_1 = 0U,
#endif
#if PLATFORM_ENABLE_SPI2
  PLATFORM_SPI_2 = 1U,
#endif
#if PLATFORM_ENABLE_SPI3
  PLATFORM_SPI_3 = 2U,
#endif
} Platform_SPI_Instance;

/** @brief SPI 身份槽位数量上限。 */
#define PLATFORM_SPI_INSTANCE_COUNT 3U

/** @brief CubeMX 为 SPI 实例配置的数据宽度。 */
typedef enum {
  PLATFORM_SPI_DATA_WIDTH_8_BIT = 8U,
  PLATFORM_SPI_DATA_WIDTH_16_BIT = 16U,
} Platform_SPI_DataWidth;

/** @brief SPI 错误标志，可同时置位多个位。 */
typedef enum {
  PLATFORM_SPI_ERROR_NONE    = 0u,
  PLATFORM_SPI_ERROR_MODF    = (1u << 0),
  PLATFORM_SPI_ERROR_CRC     = (1u << 1),
  PLATFORM_SPI_ERROR_OVR     = (1u << 2),
  PLATFORM_SPI_ERROR_FRE     = (1u << 3),
  PLATFORM_SPI_ERROR_DMA     = (1u << 4),
  PLATFORM_SPI_ERROR_FLAG    = (1u << 5),
  PLATFORM_SPI_ERROR_ABORT   = (1u << 6),
  PLATFORM_SPI_ERROR_UNKNOWN = (1u << 7),
} Platform_SPI_Error;

/** @brief SPI 异步事件。 */
typedef enum {
  PLATFORM_SPI_EVENT_TX_CPLT = 0U,
  PLATFORM_SPI_EVENT_RX_CPLT,
  PLATFORM_SPI_EVENT_TXRX_CPLT,
  PLATFORM_SPI_EVENT_ERROR,
  PLATFORM_SPI_EVENT_ABORT_CPLT,
} Platform_SPI_EventType;

/** @brief SPI 事件数据，只在回调期间有效。 */
typedef struct {
  Platform_SPI_EventType type;
  uint32_t error;
} Platform_SPI_Event;

typedef void (*Platform_SPI_Callback)(
  Platform_SPI_Instance spi,
  const Platform_SPI_Event *event,
  void *context
);

/**
 * @brief SPI 回调集合。
 * @note 回调在中断上下文执行。
 * @note 一个物理实例在整个生命周期内只能有一个回调所有者。
 */
typedef struct {
  Platform_SPI_Callback callback;
  void *context;
} platform_spi_callbacks_t;

/**
 * @brief 读取 CubeMX 配置的数据宽度。
 * @param spi SPI 实例。
 * @param data_width 输出数据宽度。
 * @note 即使工作在 16 位模式，传输函数中的长度仍以字节为单位。
 * @retval Platform_OK 查询成功。
 * @retval Platform_INVALID_ARGUMENT 实例、输出指针或 CubeMX 数据宽度非法。
 */
Platform_StatusTypeDef Platform_SPI_GetDataWidth(
  Platform_SPI_Instance spi,
  Platform_SPI_DataWidth *data_width
);

/**
 * @brief 阻塞式主机发送。
 * @param spi SPI 实例。
 * @param data 大小为 size 字节的缓冲区，调用期间只读。
 * @param size 字节数。16 位模式下必须为偶数。
 * @param timeout 超时时间（ms）或 PLATFORM_TIMEOUT_INFINITE。
 * @pre MX_SPIx_Init() 已完成，且调用方已拉低所需片选。
 */
Platform_StatusTypeDef Platform_SPI_Transmit(
  Platform_SPI_Instance spi,
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
Platform_StatusTypeDef Platform_SPI_Receive(
  Platform_SPI_Instance spi,
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
Platform_StatusTypeDef Platform_SPI_TransmitReceive(
  Platform_SPI_Instance spi,
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
Platform_StatusTypeDef Platform_SPI_Transmit_IT(
  Platform_SPI_Instance spi,
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
Platform_StatusTypeDef Platform_SPI_Receive_IT(
  Platform_SPI_Instance spi,
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
Platform_StatusTypeDef Platform_SPI_TransmitReceive_IT(
  Platform_SPI_Instance spi,
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
Platform_StatusTypeDef Platform_SPI_Transmit_DMA(
  Platform_SPI_Instance spi,
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
Platform_StatusTypeDef Platform_SPI_Receive_DMA(
  Platform_SPI_Instance spi,
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
Platform_StatusTypeDef Platform_SPI_TransmitReceive_DMA(
  Platform_SPI_Instance spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
);

/**
 * @brief 同步中止当前传输并清除错误快照。
 * @param spi SPI 实例。
 * @note 只能从任务上下文调用，禁止在 ISR 中调用。
 */
Platform_StatusTypeDef Platform_SPI_Abort(Platform_SPI_Instance spi);

/**
 * @brief 使用中断方式中止当前传输。
 * @param spi SPI 实例。
 * @note 中止完成回调在中断上下文执行。
 */
Platform_StatusTypeDef Platform_SPI_Abort_IT(Platform_SPI_Instance spi);

/**
 * @brief 读取最近一次 HAL SPI 错误快照，并转换为 Platform_SPI_Error 位。
 * @param spi SPI 实例。
 */
uint32_t Platform_SPI_GetLastError(Platform_SPI_Instance spi);

/**
 * @brief 注册 SPI 回调。
 * @param spi SPI 实例。
 * @param callbacks 待复制的回调集合。
 * @note 结构体会被复制。应在初始化阶段、启动 IT/DMA 传输前调用一次。
 * @retval Platform_OK 注册成功。
 * @retval Platform_BUSY 该实例已注册回调。
 * @retval Platform_INVALID_ARGUMENT 实例或回调集合非法。
 */
Platform_StatusTypeDef Platform_SPI_RegisterCallbacks(
  Platform_SPI_Instance spi,
  const platform_spi_callbacks_t *callbacks
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_SPI_H
