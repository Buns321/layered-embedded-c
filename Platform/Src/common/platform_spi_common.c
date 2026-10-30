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
 * @param spi 已通过后端校验的 SPI 实例；用于选择软件错误槽位。
 * @param status 后端返回的公共操作状态。
 * @return 原样返回 status，便于调用方直接转交。
 * @pre spi 必须小于 PLATFORM_SPI_INSTANCE_COUNT，否则会越界访问错误数组。
 * @post status 非 OK 时，将 platform_error_from_status(status) 写入该实例错误快照；
 *       成功时不修改快照。
 * @note 内部辅助函数；不分配内存、不阻塞，快照并发限制由 platform_error_record() 决定。
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
 * @param spi 待报告的 SPI 实例；允许越界值。
 * @return 始终返回 PLATFORM_STATUS_INVALID_ARGUMENT。
 * @post spi 位于槽位范围内时记录 PLATFORM_ERROR_INVALID_ARGUMENT；越界值不访问数组。
 * @note 内部辅助函数；不访问 SPI 硬件、不分配内存、不阻塞。
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
 * @param spi 待校验的 SPI 实例；必须启用并映射到 HAL 句柄。
 * @param data 数据缓冲区指针；必须非 NULL，访问权限由传输方向决定。
 * @param size 传输字节数；必须大于 0。
 * @retval true 实例和缓冲区有效，且 16 位模式下 size 为偶数、地址按 2 字节对齐。
 * @retval false 任一基本字段非法、数据宽度查询失败或 16 位对齐要求不满足。
 * @post 纯查询；不访问总线、不修改输入对象或模块状态。
 * @note 16 位模式按 CubeMX 的 HAL DataSize 判定；长度始终以字节数传入。
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
 * @param spi SPI 实例；非法或未注册时无操作。
 * @param type 事件类型；ERROR 事件附带后端当前错误位，其他事件错误位为 NONE。
 * @post 已注册且 callback 非 NULL 时同步调用一次；事件对象只在回调期间有效。
 * @warning 由 HAL 中断回调调用，执行上下文为中断；不得阻塞或分配内存。
 * @note 不保留事件或缓冲区指针；一个物理实例只有一个回调所有者。
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
 * @param spi SPI 实例；当前仅有效枚举值可读取后端错误。
 * @retval PLATFORM_ERROR_INVALID_ARGUMENT spi 越界。
 * @post 合法实例返回软件错误位与 HAL 错误映射的按位或；非法实例不访问快照。
 * @note 只读，不清除软件或 HAL 错误；不停止传输、不修改缓冲区，不分配内存。
 */
uint32_t Platform_SPI_GetLastError(platform_spi_instance_t spi) {
  if ((uint32_t)spi >= PLATFORM_SPI_INSTANCE_COUNT) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return spi_error[spi] | platform_spi_backend_get_error(spi);
}

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
