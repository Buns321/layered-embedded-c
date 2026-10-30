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
 * @param spi SPI 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param data_width 输出地址；不得为 NULL，成功时写入 8 位或 16 位枚举值。
 * @retval PLATFORM_STATUS_OK 查询成功。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、输出指针或 CubeMX 数据宽度非法。
 * @pre data_width 指向本次调用内可写的存储。
 * @post 成功时只写 *data_width；失败时其内容不变，传输状态也不变。
 * @note 只读取 HAL 句柄配置，不访问总线；即使工作在 16 位模式，传输函数的长度仍以字节计。
 */
platform_status_t Platform_SPI_GetDataWidth(
  platform_spi_instance_t spi,
  platform_spi_data_width_t *data_width
);

/**
 * @brief 阻塞式主机发送。
 * @param spi SPI 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param data 发送缓冲区；不得为 NULL，调用期间只读且至少包含 size 字节。
 * @param size 字节数，范围 1..UINT16_MAX；16 位模式下必须为偶数。
 * @param timeout 超时时间，单位 ms；PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 传输完成。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY SPI 正忙。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内未完成。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件报告传输失败。
 * @pre MX_SPIx_Init() 已完成，且调用方已拉低所需片选。
 * @post 成功时 size 字节已发送；本层不复制或释放缓冲区，也不控制片选。
 * @warning 仅支持主机模式；只能在任务上下文调用，同一实例的事务必须串行化。
 * @note 16 位模式下 data 地址必须按 2 字节对齐；不分配内存、不主动控制 CS。
 */
platform_status_t Platform_SPI_Transmit(
  platform_spi_instance_t spi,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief 阻塞式主机接收。
 * @param spi SPI 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param data 接收缓冲区；不得为 NULL，调用期间可写且容量至少为 size 字节。
 * @param size 字节数，范围 1..UINT16_MAX；16 位模式下必须为偶数。
 * @param timeout 超时时间，单位 ms；PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 传输完成。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY SPI 正忙。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内未完成。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件报告传输失败。
 * @pre MX_SPIx_Init() 已完成，且调用方已拉低所需片选。
 * @post 成功时 data 前 size 字节由硬件填充；失败时内容不确定。
 * @warning 仅支持主机模式；只能在任务上下文调用，同一实例的事务必须串行化。
 * @note 16 位模式下 data 地址必须按 2 字节对齐；不分配内存、不主动控制 CS。
 */
platform_status_t Platform_SPI_Receive(
  platform_spi_instance_t spi,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief 阻塞式全双工主机传输。
 * @param spi SPI 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param tx_data 发送缓冲区；不得为 NULL，调用期间只读且至少包含 size 字节。
 * @param rx_data 接收缓冲区；不得为 NULL，调用期间可写且容量至少为 size 字节。
 * @param size 每个方向的字节数，范围 1..UINT16_MAX；16 位模式下必须为偶数。
 * @param timeout 超时时间，单位 ms；PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 发送和接收同时完成。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、任一缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY SPI 正忙。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内未完成。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件报告传输失败。
 * @pre MX_SPIx_Init() 已完成，且调用方已拉低所需片选。
 * @post 成功时 size 字节已发送并写入 rx_data；失败时 rx_data 内容不确定。
 * @warning 仅支持主机模式；只能在任务上下文调用，两个缓冲区都必须覆盖整个调用。
 * @note 16 位模式下 size 为偶数且两个地址均按 2 字节对齐；不分配内存、不控制 CS。
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
 * @param spi SPI 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param data 发送缓冲区；不得为 NULL，至少包含 size 字节。
 * @param size 字节数，范围 1..UINT16_MAX；16 位模式下必须为偶数。
 * @retval PLATFORM_STATUS_OK HAL 已接受中断发送请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY SPI 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件无法启动请求。
 * @pre MX_SPIx_Init() 和 NVIC 已完成配置；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受，最终结果通过完成、错误或中止事件通知。
 * @warning 在完成、错误或中止事件前，data 必须保持有效且内容不变；回调在中断上下文执行。
 * @note 仅支持主机模式，不控制 CS；16 位模式下 data 地址必须按 2 字节对齐。
 */
platform_status_t Platform_SPI_TransmitIT(
  platform_spi_instance_t spi,
  const uint8_t *data,
  uint16_t size
);

/**
 * @brief 中断方式主机接收。
 * @param spi SPI 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param data 接收缓冲区；不得为 NULL，容量至少为 size 字节。
 * @param size 字节数，范围 1..UINT16_MAX；16 位模式下必须为偶数。
 * @retval PLATFORM_STATUS_OK HAL 已接受中断接收请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY SPI 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件无法启动请求。
 * @pre MX_SPIx_Init() 和 NVIC 已完成配置；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受；完成前不得读取完整 data，失败时内容不确定。
 * @warning 在完成、错误或中止事件前，data 必须保持有效且不得并发访问；回调在中断上下文执行。
 * @note 仅支持主机模式，不控制 CS；16 位模式下 data 地址必须按 2 字节对齐。
 */
platform_status_t Platform_SPI_ReceiveIT(
  platform_spi_instance_t spi,
  uint8_t *data,
  uint16_t size
);

/**
 * @brief 中断方式全双工主机传输。
 * @param spi SPI 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param tx_data 发送缓冲区；不得为 NULL，至少包含 size 字节。
 * @param rx_data 接收缓冲区；不得为 NULL，容量至少为 size 字节。
 * @param size 每个方向的字节数，范围 1..UINT16_MAX；16 位模式下必须为偶数。
 * @retval PLATFORM_STATUS_OK HAL 已接受中断全双工请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、任一缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY SPI 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件无法启动请求。
 * @pre MX_SPIx_Init() 和 NVIC 已完成配置；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受；完成前不得读取完整 rx_data，失败时其内容不确定。
 * @warning 在完成、错误或中止事件前，两个缓冲区都必须保持有效；回调在中断上下文执行。
 * @note 仅支持主机模式，不控制 CS；16 位模式下两个地址均按 2 字节对齐。
 */
platform_status_t Platform_SPI_TransmitReceiveIT(
  platform_spi_instance_t spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
);

/**
 * @brief DMA 方式主机发送。
 * @param spi SPI 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param data 发送缓冲区；不得为 NULL，至少包含 size 字节。
 * @param size 字节数，范围 1..UINT16_MAX；16 位模式下必须为偶数。
 * @retval PLATFORM_STATUS_OK HAL 已接受 DMA 发送请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY SPI 或 DMA 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或 DMA 无法启动请求。
 * @pre MX_SPIx_Init() 和 CubeMX TX DMA 配置已完成；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受，最终结果通过完成、错误或中止事件通知。
 * @warning 在完成、错误或中止事件前，data 必须保持有效且内容不变；回调在中断上下文执行。
 * @note 仅支持主机模式，不控制 CS；16 位模式下 data 地址必须按 2 字节对齐。
 */
platform_status_t Platform_SPI_TransmitDMA(
  platform_spi_instance_t spi,
  const uint8_t *data,
  uint16_t size
);

/**
 * @brief DMA 方式主机接收。
 * @param spi SPI 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param data 接收缓冲区；不得为 NULL，容量至少为 size 字节。
 * @param size 字节数，范围 1..UINT16_MAX；16 位模式下必须为偶数。
 * @retval PLATFORM_STATUS_OK HAL 已接受 DMA 接收请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY SPI 或 DMA 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或 DMA 无法启动请求。
 * @pre MX_SPIx_Init() 和 CubeMX RX DMA 配置已完成；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受；完成前不得读取完整 data，失败时内容不确定。
 * @warning 在完成、错误或中止事件前，data 必须保持有效且不得并发访问；回调在中断上下文执行。
 * @note 仅支持主机模式，不控制 CS；16 位模式下 data 地址必须按 2 字节对齐。
 */
platform_status_t Platform_SPI_ReceiveDMA(
  platform_spi_instance_t spi,
  uint8_t *data,
  uint16_t size
);

/**
 * @brief DMA 方式全双工主机传输。
 * @param spi SPI 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param tx_data 发送缓冲区；不得为 NULL，至少包含 size 字节。
 * @param rx_data 接收缓冲区；不得为 NULL，容量至少为 size 字节。
 * @param size 每个方向的字节数，范围 1..UINT16_MAX；16 位模式下必须为偶数。
 * @retval PLATFORM_STATUS_OK HAL 已接受 DMA 全双工请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、任一缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY SPI 或 DMA 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或 DMA 无法启动请求。
 * @pre MX_SPIx_Init() 和 CubeMX TX/RX DMA 配置已完成；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受；完成前不得读取完整 rx_data，失败时其内容不确定。
 * @warning 在完成、错误或中止事件前，两个缓冲区都必须保持有效；回调在中断上下文执行。
 * @note 仅支持主机模式，不控制 CS；16 位模式下两个地址均按 2 字节对齐。
 */
platform_status_t Platform_SPI_TransmitReceiveDMA(
  platform_spi_instance_t spi,
  const uint8_t *tx_data,
  uint8_t *rx_data,
  uint16_t size
);

/**
 * @brief 同步中止当前传输并清除错误快照。
 * @param spi SPI 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @retval PLATFORM_STATUS_OK 中止完成，硬件传输状态和 HAL 错误码已清除。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界或未启用；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告实例或总线忙。
 * @retval PLATFORM_STATUS_ERROR HAL 中止失败；不得假定总线已经空闲。
 * @pre 调用方已启动需要中止的传输，并确保没有同一实例的并发启动或回调操作。
 * @post 成功时可在无在途事件后重新启动传输；失败时保留 HAL 报告的硬件状态。
 * @warning 只能从任务上下文调用，禁止在 ISR 中调用；内部可能同步等待 HAL 完成中止。
 * @note 调用开始时清除软件错误快照；不释放缓冲区，也不控制 CS。
 */
platform_status_t Platform_SPI_Abort(platform_spi_instance_t spi);

/**
 * @brief 使用中断方式中止当前传输。
 * @param spi SPI 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @retval PLATFORM_STATUS_OK HAL 已接受异步中止请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界或未启用；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告实例或总线忙。
 * @retval PLATFORM_STATUS_ERROR HAL 拒绝中止或无法启动中止序列。
 * @pre 目标实例存在可中止的异步传输；如需事件，回调已注册，且调用发生在任务上下文。
 * @post 返回 OK 只表示中止已启动，最终结果通过 ABORT_COMPLETE 事件通知。
 * @warning 在完成、错误或中止事件处理结束前，不得复用缓冲区或启动新传输。
 * @note 中止完成回调在中断上下文执行；本函数不直接清除 HAL 错误码。
 */
platform_status_t Platform_SPI_AbortIT(platform_spi_instance_t spi);

/**
 * @brief 读取最近一次 HAL SPI 错误快照，并转换为 platform_spi_error_t 位。
 * @param spi SPI 实例；当前仅有效枚举值可读取后端错误。
 * @retval PLATFORM_ERROR_INVALID_ARGUMENT spi 越界。
 * @post 合法实例返回软件错误位与 HAL 错误映射的按位或；非法实例不访问快照。
 * @note 只读，不清除软件或 HAL 错误；不停止传输、不修改缓冲区，不分配内存。
 */
uint32_t Platform_SPI_GetLastError(platform_spi_instance_t spi);

/**
 * @brief 注册 SPI 回调。
 * @param spi SPI 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param callbacks 待复制的回调集合；不得为 NULL，callback 成员可为 NULL 以禁用通知。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该实例已注册回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或回调集合非法。
 * @pre callbacks 指向的对象在调用期间有效；每个实例仅允许成功注册一次。
 * @post 成功时复制 callback 和 context，后续完成、错误或中止事件调用该回调。
 * @warning 回调在中断上下文执行；事件对象只在回调期间有效，禁止阻塞或分配内存。
 * @note 应在初始化阶段、启动 IT/DMA 传输前注册；context 的生命周期由调用方维护。
 */
platform_status_t Platform_SPI_RegisterCallback(
  platform_spi_instance_t spi,
  const platform_spi_callback_binding_t *callbacks
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_SPI_H
