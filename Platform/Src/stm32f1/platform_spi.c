/**
 * @file platform_spi.c
 * @brief STM32F1 公共 SPI 实现的 HAL 后端。
 * @ingroup platform
 */

#include "platform_hal_internal.h"
#include "platform_spi_backend.h"
#include "spi.h"
#include "stm32f1xx_hal.h"

/**
 * @brief 将平台 SPI 实例映射为 CubeMX 生成的 HAL 句柄。
 * @param spi SPI 实例编号；应为 platform_spi.h 中由 PLATFORM_ENABLE_SPIx 启用的枚举值。
 * @return 对应句柄指针；未启用或非法实例返回 NULL。
 * @pre 对启用实例，CubeMX 必须已生成并初始化对应 SPI_HandleTypeDef 全局句柄。
 * @post 成功时返回指向全局 HAL 句柄的长期有效指针；函数不改句柄内容，也不操作硬件。
 * @note 仅在本编译单元内调用；映射表为编译期静态数据，可在任务或 ISR 上下文只读调用。
 */
static SPI_HandleTypeDef *platform_spi_to_hal(platform_spi_instance_t spi) {
  switch (spi) {
#if PLATFORM_ENABLE_SPI1
    case PLATFORM_SPI_1: return &hspi1;
#endif
#if PLATFORM_ENABLE_SPI2
    case PLATFORM_SPI_2: return &hspi2;
#endif
    default: return NULL;
  }
}

/**
 * @brief 将 HAL SPI 句柄识别为平台实例编号。
 * @param spi_h HAL SPI 句柄；可为 NULL，NULL 时不匹配。
 * @param spi 输出实例编号；可为 NULL，NULL 时不匹配；成功时写入启用实例。
 * @retval true spi_h 与某个已启用实例的 CubeMX 句柄匹配，且 *spi 已写入。
 * @retval false 任一指针为 NULL、句柄不属于本板启用实例，或实例未启用。
 * @post 返回 false 时不写 *spi；成功时返回的实例可直接用于后端映射。
 * @note 仅在本编译单元内调用；只比较全局句柄地址，不访问硬件，可在 ISR 上下文调用。
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
  return false;
}

/**
 * @brief 将字节长度转换为 HAL 传输帧数。
 * @param spi_h 已初始化的 SPI HAL 句柄；不得为 NULL，读取其 Init.DataSize。
 * @param size 公共接口传入的字节数；16 位模式下应为偶数。
 * @return 8 位模式返回 size；16 位模式返回 size/2 的截断结果。
 * @pre 公共层已按数据宽度校验 size 为偶数；spi_h 的 Init.DataSize 已由 CubeMX 配置。
 * @post 只读句柄配置，不修改缓冲区、句柄或硬件状态。
 * @note 仅在本编译单元内调用；不做错误处理，奇数字节在 16 位模式下会被向下取整。
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
 * @brief 将 HAL SPI 错误位映射为平台 SPI 错误位。
 * @param hal_error HAL SPI 错误码位掩码；允许同时置位多个已知或未知错误。
 * @retval PLATFORM_SPI_ERROR_NONE hal_error 为 0 时。
 * @post 不修改 HAL 错误寄存器、句柄错误码或传输状态。
 * @note 仅在本编译单元内调用；纯位映射，可在任务或 ISR 上下文调用。
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
  if ((hal_error & HAL_SPI_ERROR_DMA) != 0U) {
    error |= PLATFORM_SPI_ERROR_DMA;
  }
  if ((hal_error & HAL_SPI_ERROR_FLAG) != 0U) {
    error |= PLATFORM_SPI_ERROR_FLAG;
  }
  if ((hal_error & HAL_SPI_ERROR_ABORT) != 0U) {
    error |= PLATFORM_SPI_ERROR_ABORT;
  }

  const uint32_t known_errors =
    HAL_SPI_ERROR_MODF |
    HAL_SPI_ERROR_CRC |
    HAL_SPI_ERROR_OVR |
    HAL_SPI_ERROR_DMA |
    HAL_SPI_ERROR_FLAG |
    HAL_SPI_ERROR_ABORT;
  if ((hal_error & ~known_errors) != 0U) {
    error |= PLATFORM_SPI_ERROR_UNKNOWN;
  }
  return error;
}

/**
 * @brief 校验 SPI 实例是否映射到已启用的 HAL 句柄。
 * @param spi SPI 实例编号；应为 platform_spi.h 中由 PLATFORM_ENABLE_SPIx 启用的枚举值。
 * @retval true 实例在身份槽位范围内且存在对应的 CubeMX HAL 句柄。
 * @retval false 实例越界、未启用或没有对应的 HAL 句柄。
 * @post 不修改实例、HAL 句柄或硬件状态。
 * @note 只读静态映射，可在任务或中断上下文调用；不分配内存，不访问外设寄存器。
 */
bool platform_spi_backend_is_valid(platform_spi_instance_t spi) {
  return (uint32_t)spi < PLATFORM_SPI_INSTANCE_COUNT &&
         platform_spi_to_hal(spi) != NULL;
}

/**
 * @brief 从 CubeMX 配置中读取 SPI 数据宽度。
 * @param spi SPI 实例编号；必须映射到已启用的 HAL 句柄。
 * @param data_width 输出参数，接收 8 位或 16 位数据宽度；不得为 NULL。
 * @retval PLATFORM_STATUS_OK 查询成功，*data_width 已写入有效宽度。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法、data_width 为 NULL，或 HAL 配置不是 8/16 位。
 * @pre CubeMX 已生成并初始化对应 SPI_HandleTypeDef；data_width 指向本次调用内可写的存储。
 * @post 成功时只写 *data_width；失败时不修改其内容，也不改变传输状态。
 * @note 只读句柄的 Init.DataSize，不访问总线；实例并发访问由上层串行化。
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
 * @brief 以阻塞方式向 SPI 主机总线发送数据。
 * @param spi SPI 实例编号；必须映射到已启用的 HAL 句柄。
 * @param data 发送缓冲区；不得为 NULL，调用期间保持只读且有效。
 * @param size 发送字节数，范围 1..65535；16 位模式下必须为偶数。
 * @param timeout 超时时间，单位 ms；可为 PLATFORM_TIMEOUT_INFINITE。
 * @return 操作状态；HAL 结果映射为 OK、ERROR、BUSY、TIMEOUT 或实例非法时的 INVALID_ARGUMENT。
 * @pre CubeMX 已完成 SPI 初始化，调用方已按设备时序拉低片选；16 位模式缓冲区按 16 位对齐。
 * @post 成功返回表示传输完成；任何返回都不释放或复制调用方缓冲区。
 * @note 仅在任务上下文调用且会阻塞；同一实例的并发事务必须由上层串行化。函数不控制片选。
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
 * @brief 以阻塞方式从 SPI 主机总线接收数据。
 * @param spi SPI 实例编号；必须映射到已启用的 HAL 句柄。
 * @param data 接收缓冲区；不得为 NULL，调用期间保持可写且有效。
 * @param size 接收字节数，范围 1..65535；16 位模式下必须为偶数。
 * @param timeout 超时时间，单位 ms；可为 PLATFORM_TIMEOUT_INFINITE。
 * @return 操作状态；HAL 结果映射为 OK、ERROR、BUSY、TIMEOUT 或实例非法时的 INVALID_ARGUMENT。
 * @pre CubeMX 已完成 SPI 初始化，调用方已按设备时序拉低片选；16 位模式缓冲区按 16 位对齐。
 * @post 成功返回时 data 的前 size 字节已由硬件填充；失败时缓冲区内容不确定。
 * @note 仅在任务上下文调用且会阻塞；同一实例的并发事务必须由上层串行化。函数不控制片选。
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
 * @brief 以阻塞方式执行 SPI 全双工主机传输。
 * @param spi SPI 实例编号；必须映射到已启用的 HAL 句柄。
 * @param tx_data 发送缓冲区；不得为 NULL，调用期间保持只读且有效。
 * @param rx_data 接收缓冲区；不得为 NULL，调用期间保持可写且有效。
 * @param size 每个方向的传输字节数，范围 1..65535；16 位模式下必须为偶数。
 * @param timeout 超时时间，单位 ms；可为 PLATFORM_TIMEOUT_INFINITE。
 * @return 操作状态；HAL 结果映射为 OK、ERROR、BUSY、TIMEOUT 或实例非法时的 INVALID_ARGUMENT。
 * @pre CubeMX 已完成 SPI 初始化，调用方已按设备时序拉低片选；16 位模式两个缓冲区均按 16 位对齐。
 * @post 成功返回表示 size 字节发送并接收完成；函数不复制或释放任一缓冲区。
 * @note 仅在任务上下文调用且会阻塞；同一实例的并发事务必须由上层串行化。函数不控制片选。
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
 * @brief 启动 SPI 中断发送。
 * @param spi SPI 实例编号；必须映射到已启用的 HAL 句柄。
 * @param data 发送缓冲区；不得为 NULL，从启动成功到最终事件期间保持有效且不被修改。
 * @param size 发送字节数，范围 1..65535；16 位模式下必须为偶数。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或 HAL 句柄不存在。
 * @pre 回调已注册，CubeMX 已配置 SPI 中断和 NVIC；16 位模式缓冲区按 16 位对齐。
 * @post 成功时实例进入忙碌状态，后续由完成、错误或中止事件结束本次传输。
 * @note 从任务上下文启动；回调和状态更新在中断上下文执行，禁止在 ISR 中调用本函数。
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
 * @brief 启动 SPI 中断接收。
 * @param spi SPI 实例编号；必须映射到已启用的 HAL 句柄。
 * @param data 接收缓冲区；不得为 NULL，从启动成功到最终事件期间保持可写且有效。
 * @param size 接收字节数，范围 1..65535；16 位模式下必须为偶数。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或 HAL 句柄不存在。
 * @pre 回调已注册，CubeMX 已配置 SPI 中断和 NVIC；16 位模式缓冲区按 16 位对齐。
 * @post 成功时实例进入忙碌状态，后续由完成、错误或中止事件结束本次传输。
 * @note 从任务上下文启动；完成回调在中断上下文执行，Platform 不自动重新挂起接收。
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
 * @param spi SPI 实例编号；必须映射到已启用的 HAL 句柄。
 * @param tx_data 发送缓冲区；不得为 NULL，从启动成功到最终事件期间保持只读且有效。
 * @param rx_data 接收缓冲区；不得为 NULL，从启动成功到最终事件期间保持可写且有效。
 * @param size 每个方向的传输字节数，范围 1..65535；16 位模式下必须为偶数。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或 HAL 句柄不存在。
 * @pre 回调已注册，CubeMX 已配置 SPI 中断和 NVIC；16 位模式两个缓冲区均按 16 位对齐。
 * @post 成功时实例进入忙碌状态，后续由完成、错误或中止事件结束本次传输。
 * @note 从任务上下文启动；完成回调在中断上下文执行，禁止在 ISR 中调用本函数。
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
 * @brief 启动 SPI DMA 发送。
 * @param spi SPI 实例编号；必须映射到已启用的 HAL 句柄。
 * @param data 发送缓冲区；不得为 NULL，从启动成功到最终事件期间保持有效且不被修改。
 * @param size 发送字节数，范围 1..65535；16 位模式下必须为偶数。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或 HAL 句柄不存在。
 * @pre 回调已注册，CubeMX 已配置 SPI TX DMA stream/channel 和 NVIC；缓冲区满足 DMA 可访问性。
 * @post 成功时 DMA 持有缓冲区地址，实例保持忙碌直到完成、错误或中止事件。
 * @note 从任务上下文启动；回调在中断上下文执行，Platform 不复制缓冲区且不自动重启。
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
 * @brief 启动 SPI DMA 接收。
 * @param spi SPI 实例编号；必须映射到已启用的 HAL 句柄。
 * @param data 接收缓冲区；不得为 NULL，从启动成功到最终事件期间保持可写且有效。
 * @param size 接收字节数，范围 1..65535；16 位模式下必须为偶数。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或 HAL 句柄不存在。
 * @pre 回调已注册，CubeMX 已配置 SPI RX DMA stream/channel 和 NVIC；缓冲区满足 DMA 可访问性。
 * @post 成功时 DMA 持有缓冲区地址，实例保持忙碌直到完成、错误或中止事件。
 * @note 从任务上下文启动；完成回调在中断上下文执行，Platform 不复制缓冲区且不自动重新挂起。
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
 * @param spi SPI 实例编号；必须映射到已启用的 HAL 句柄。
 * @param tx_data 发送缓冲区；不得为 NULL，从启动成功到最终事件期间保持只读且有效。
 * @param rx_data 接收缓冲区；不得为 NULL，从启动成功到最终事件期间保持可写且有效。
 * @param size 每个方向的传输字节数，范围 1..65535；16 位模式下必须为偶数。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或 HAL 句柄不存在。
 * @pre 回调已注册，CubeMX 已配置 SPI TX/RX DMA stream/channel 和 NVIC；缓冲区满足 DMA 可访问性。
 * @post 成功时 DMA 持有两个缓冲区地址，实例保持忙碌直到完成、错误或中止事件。
 * @note 从任务上下文启动；回调在中断上下文执行，Platform 不复制缓冲区且不自动重启。
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
 * @brief 同步中止 SPI 当前异步传输。
 * @param spi SPI 实例编号；必须映射到已启用的 HAL 句柄。
 * @retval PLATFORM_STATUS_OK 中止完成，硬件传输状态和 HAL 错误码已清除。
 * @pre 仅在任务上下文调用；不得与同一实例的启动、回调或其他 abort 操作并发。
 * @post 成功时可显式重新启动传输；失败时保留 HAL 报告的状态，调用方不得假定总线已空闲。
 * @note 同步接口可能等待 HAL 完成中止；不要在中断上下文调用。
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
 * @brief 启动 SPI 异步中止。
 * @param spi SPI 实例编号；必须映射到已启用的 HAL 句柄。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或 HAL 句柄不存在。
 * @pre 回调已注册且存在可中止的异步传输；从任务上下文调用，避免与完成回调并发启动新事务。
 * @post 成功时硬件中止过程继续，最终通过 PLATFORM_SPI_EVENT_ABORT_COMPLETE 结束。
 * @note 中止完成回调在中断上下文执行；本函数不直接清除 HAL 错误码。
 */
platform_status_t platform_spi_backend_abort_it(
  platform_spi_instance_t spi
) {
  SPI_HandleTypeDef *spi_h = platform_spi_to_hal(spi);
  return spi_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_SPI_Abort_IT(spi_h));
}

/**
 * @brief 读取 SPI HAL 错误快照并转换为平台错误位。
 * @param spi SPI 实例编号；应为已启用的平台枚举值。
 * @retval PLATFORM_SPI_ERROR_NONE HAL 未记录已知错误。
 * @retval PLATFORM_SPI_ERROR_UNKNOWN 实例非法/未映射，或 HAL 返回未识别错误位。
 * @post 只读 HAL 错误码，不清除错误，也不改变传输状态或硬件寄存器。
 * @note 可在任务或中断上下文调用；与其他实例查询互不共享可变状态。
 */
uint32_t platform_spi_backend_get_error(platform_spi_instance_t spi) {
  SPI_HandleTypeDef *spi_h = platform_spi_to_hal(spi);
  return spi_h == NULL ? PLATFORM_SPI_ERROR_UNKNOWN
                   : platform_spi_map_error(HAL_SPI_GetError(spi_h));
}

/**
 * @brief 将 HAL SPI 回调桥接到平台事件分发器。
 * @param spi_h HAL 传入的 SPI 句柄；可为 NULL 或未启用的句柄，此时忽略事件。
 * @param type HAL 回调对应的平台事件类型。
 * @post 识别到启用实例时同步转发事件；否则不调用上层回调，也不修改硬件。
 * @note 由 HAL 中断回调调用；不得阻塞、分配内存或执行长耗时操作。
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
 * @brief 处理 SPI 中断/DMA 发送完成回调。
 * @param spi_h HAL 传入的 SPI 句柄；可忽略 NULL 或未启用实例。
 * @post 向已注册平台回调发送 PLATFORM_SPI_EVENT_TX_COMPLETE；不自动启动后续传输。
 * @note 在 HAL 中断上下文调用；必须保持短小且不可阻塞。
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *spi_h) {
  platform_spi_dispatch_hal_event(spi_h, PLATFORM_SPI_EVENT_TX_COMPLETE);
}

/**
 * @brief 处理 SPI 接收完成回调。
 * @param spi_h HAL 传入的 SPI 句柄；可忽略 NULL 或未启用实例。
 * @post 向已注册平台回调发送 PLATFORM_SPI_EVENT_RX_COMPLETE；接收缓冲区由调用方继续保管。
 * @note 在 HAL 中断上下文调用；不得阻塞、分配内存或执行长耗时处理。
 */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *spi_h) {
  platform_spi_dispatch_hal_event(spi_h, PLATFORM_SPI_EVENT_RX_COMPLETE);
}

/**
 * @brief 处理 SPI 中断/DMA 全双工完成回调。
 * @param spi_h HAL 传入的 SPI 句柄；可忽略 NULL 或未启用实例。
 * @post 向已注册平台回调发送 PLATFORM_SPI_EVENT_TX_RX_COMPLETE；不自动重启传输。
 * @note 在 HAL 中断上下文调用；必须保持短小且不可阻塞。
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *spi_h) {
  platform_spi_dispatch_hal_event(spi_h, PLATFORM_SPI_EVENT_TX_RX_COMPLETE);
}

/**
 * @brief 处理 SPI 错误回调并转发错误事件。
 * @param spi_h HAL 传入的 SPI 句柄；可忽略 NULL 或未启用实例。
 * @post 平台事件分发器读取 HAL 错误快照并发送 PLATFORM_SPI_EVENT_ERROR；错误码在分发时读取。
 * @note 在 HAL 中断上下文调用；不得阻塞、分配内存或执行长耗时处理。
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *spi_h) {
  platform_spi_dispatch_hal_event(spi_h, PLATFORM_SPI_EVENT_ERROR);
}

/**
 * @brief 处理 SPI 异步中止完成回调。
 * @param spi_h HAL 传入的 SPI 句柄；可忽略 NULL 或未启用实例。
 * @post 向已注册平台回调发送 PLATFORM_SPI_EVENT_ABORT_COMPLETE；调用方可在事件后重新启动。
 * @note 在 HAL 中断上下文调用；不得阻塞、分配内存或执行长耗时处理。
 */
void HAL_SPI_AbortCpltCallback(SPI_HandleTypeDef *spi_h) {
  platform_spi_dispatch_hal_event(spi_h, PLATFORM_SPI_EVENT_ABORT_COMPLETE);
}
