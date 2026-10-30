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
 * @brief 校验 SPI 实例是否映射到已启用的 HAL 句柄。
 * @param spi SPI 实例编号；应为 platform_spi.h 中由 PLATFORM_ENABLE_SPIx 启用的枚举值。
 * @retval true 实例在身份槽位范围内且存在对应的 CubeMX HAL 句柄。
 * @retval false 实例越界、未启用或没有对应的 HAL 句柄。
 * @post 不修改实例、HAL 句柄或硬件状态。
 * @note 只读静态映射，可在任务或中断上下文调用；不分配内存，不访问外设寄存器。
 */
bool platform_spi_backend_is_valid(platform_spi_instance_t spi);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

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
);

/**
 * @brief 同步中止 SPI 当前异步传输。
 * @param spi SPI 实例编号；必须映射到已启用的 HAL 句柄。
 * @retval PLATFORM_STATUS_OK 中止完成，硬件传输状态和 HAL 错误码已清除。
 * @pre 仅在任务上下文调用；不得与同一实例的启动、回调或其他 abort 操作并发。
 * @post 成功时可显式重新启动传输；失败时保留 HAL 报告的状态，调用方不得假定总线已空闲。
 * @note 同步接口可能等待 HAL 完成中止；不要在中断上下文调用。
 */
platform_status_t platform_spi_backend_abort(platform_spi_instance_t spi);

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
);

/**
 * @brief 读取 SPI HAL 错误快照并转换为平台错误位。
 * @param spi SPI 实例编号；应为已启用的平台枚举值。
 * @retval PLATFORM_SPI_ERROR_NONE HAL 未记录已知错误。
 * @retval PLATFORM_SPI_ERROR_UNKNOWN 实例非法/未映射，或 HAL 返回未识别错误位。
 * @post 只读 HAL 错误码，不清除错误，也不改变传输状态或硬件寄存器。
 * @note 可在任务或中断上下文调用；与其他实例查询互不共享可变状态。
 */
uint32_t platform_spi_backend_get_error(platform_spi_instance_t spi);

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
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_SPI_BACKEND_H
