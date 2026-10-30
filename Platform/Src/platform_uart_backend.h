/**
 * @file platform_uart_backend.h
 * @brief STM32 系列公共 UART 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_UART_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_UART_BACKEND_H

#include "platform_def.h"
#include "platform_uart.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 校验 UART 实例是否映射到已启用的 HAL 句柄。
 * @param uart UART 实例编号；应为 platform_uart.h 中由 PLATFORM_ENABLE_UARTx 启用的枚举值。
 * @retval true 实例在身份槽位范围内且存在对应的 CubeMX HAL 句柄。
 * @retval false 实例越界、未启用或没有对应的 HAL 句柄。
 * @post 不修改实例、HAL 句柄或硬件状态。
 * @note 只读静态映射，可在任务或中断上下文调用；不分配内存，不访问外设寄存器。
 */
bool platform_uart_backend_is_valid(platform_uart_instance_t uart);

/**
 * @brief 以阻塞方式接收 UART 数据。
 * @param uart UART 实例编号；必须映射到已启用的 HAL 句柄。
 * @param data 接收缓冲区；不得为 NULL，调用期间保持可写且有效。
 * @param size 接收字节数，范围 1..65535。
 * @param timeout 超时时间，单位 ms；可为 PLATFORM_TIMEOUT_INFINITE。
 * @return 操作状态；HAL 结果映射为 OK、ERROR、BUSY、TIMEOUT 或实例非法时的 INVALID_ARGUMENT。
 * @pre CubeMX 已完成 UART 初始化；一次只允许一个阻塞事务使用该实例。
 * @post 成功返回时 data 的前 size 字节已接收；失败时已写入内容由 HAL 决定，不得假定完整。
 * @note 仅在任务上下文调用且会阻塞；Platform 不复制缓冲区，也不自动重新挂起接收。
 */
platform_status_t platform_uart_backend_receive(
  platform_uart_instance_t uart,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief 以阻塞方式发送 UART 数据。
 * @param uart UART 实例编号；必须映射到已启用的 HAL 句柄。
 * @param data 发送缓冲区；不得为 NULL，调用期间保持只读且有效。
 * @param size 发送字节数，范围 1..65535。
 * @param timeout 超时时间，单位 ms；可为 PLATFORM_TIMEOUT_INFINITE。
 * @return 操作状态；HAL 结果映射为 OK、ERROR、BUSY、TIMEOUT 或实例非法时的 INVALID_ARGUMENT。
 * @pre CubeMX 已完成 UART 初始化；一次只允许一个阻塞事务使用该实例。
 * @post 成功返回表示发送完成；函数不复制或释放调用方缓冲区。
 * @note 仅在任务上下文调用且会阻塞；同一实例的并发访问必须由上层串行化。
 */
platform_status_t platform_uart_backend_transmit(
  platform_uart_instance_t uart,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief 启动 UART 中断接收。
 * @param uart UART 实例编号；必须映射到已启用的 HAL 句柄。
 * @param data 接收缓冲区；不得为 NULL，从启动成功到最终事件期间保持可写且有效。
 * @param size 接收字节数，范围 1..65535。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或 HAL 句柄不存在。
 * @pre 回调已注册，CubeMX 已配置 UART 中断和 NVIC；同一实例无其他活动接收。
 * @post 成功时实例进入忙碌状态，后续由完成、错误或中止事件结束本次接收。
 * @note 从任务上下文启动；回调在中断上下文执行，Platform 不复制缓冲区且不自动重新挂起接收。
 */
platform_status_t platform_uart_backend_receive_it(
  platform_uart_instance_t uart,
  uint8_t *data,
  uint16_t size
);

/**
 * @brief 启动 UART 中断发送。
 * @param uart UART 实例编号；必须映射到已启用的 HAL 句柄。
 * @param data 发送缓冲区；不得为 NULL，从启动成功到最终事件期间保持只读且有效。
 * @param size 发送字节数，范围 1..65535。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或 HAL 句柄不存在。
 * @pre 回调已注册，CubeMX 已配置 UART 中断和 NVIC；同一实例无其他活动发送。
 * @post 成功时实例进入忙碌状态，后续由 TX 完成、错误或中止事件结束本次发送。
 * @note 从任务上下文启动；回调在中断上下文执行，Platform 不复制缓冲区且不自动重启发送。
 */
platform_status_t platform_uart_backend_transmit_it(
  platform_uart_instance_t uart,
  const uint8_t *data,
  uint16_t size
);

/**
 * @brief 启动 UART DMA 接收。
 * @param uart UART 实例编号；必须映射到已启用的 HAL 句柄。
 * @param data 接收缓冲区；不得为 NULL，从启动成功到最终事件期间保持可写且有效。
 * @param size 接收字节数，范围 1..65535。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或 HAL 句柄不存在。
 * @pre 回调已注册，CubeMX 已配置 UART RX DMA stream/channel 和 NVIC；缓冲区满足 DMA 可访问性。
 * @post 成功时 DMA 持有缓冲区地址，实例保持忙碌直到完成、错误或中止事件。
 * @note 从任务上下文启动；半完成事件只保证对应半区已写入，Platform 不复制缓冲区且不自动重挂。
 */
platform_status_t platform_uart_backend_receive_dma(
  platform_uart_instance_t uart,
  uint8_t *data,
  uint16_t size
);

/**
 * @brief 启动 UART DMA 发送。
 * @param uart UART 实例编号；必须映射到已启用的 HAL 句柄。
 * @param data 发送缓冲区；不得为 NULL，从启动成功到最终事件期间保持只读且有效。
 * @param size 发送字节数，范围 1..65535。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或 HAL 句柄不存在。
 * @pre 回调已注册，CubeMX 已配置 UART TX DMA stream/channel 和 NVIC；缓冲区满足 DMA 可访问性。
 * @post 成功时 DMA 持有缓冲区地址，实例保持忙碌直到 TX 完成、错误或中止事件。
 * @note 从任务上下文启动；回调在中断上下文执行，Platform 不复制缓冲区且不自动重启发送。
 */
platform_status_t platform_uart_backend_transmit_dma(
  platform_uart_instance_t uart,
  const uint8_t *data,
  uint16_t size
);

/**
 * @brief 同步中止 UART 接收并清除接收错误状态。
 * @param uart UART 实例编号；必须映射到已启用的 HAL 句柄。
 * @retval PLATFORM_STATUS_OK 接收中止完成，RX 标志和 HAL 错误码已清除。
 * @pre 仅在任务上下文调用；不得与同一实例的接收启动或其他 abort 操作并发。
 * @post 成功时可显式重新启动接收；失败时保留 HAL 报告的状态，不清除错误标志。
 * @note 同步接口可能等待 HAL 完成中止，不要在中断上下文调用。
 */
platform_status_t platform_uart_backend_abort_receive(
  platform_uart_instance_t uart
);

/**
 * @brief 同步中止 UART 发送。
 * @param uart UART 实例编号；必须映射到已启用的 HAL 句柄。
 * @retval PLATFORM_STATUS_OK 发送中止完成，HAL 错误码已清除。
 * @pre 仅在任务上下文调用；不得与同一实例的发送启动或其他 abort 操作并发。
 * @post 成功时可显式重新启动发送；失败时保留 HAL 报告的状态。
 * @note 同步接口可能等待 HAL 完成中止，不要在中断上下文调用。
 */
platform_status_t platform_uart_backend_abort_transmit(
  platform_uart_instance_t uart
);

/**
 * @brief 同步中止 UART 的发送和接收。
 * @param uart UART 实例编号；必须映射到已启用的 HAL 句柄。
 * @retval PLATFORM_STATUS_OK 双向传输已中止，RX 标志和 HAL 错误码已清除。
 * @pre 仅在任务上下文调用；不得与同一实例的传输启动或回调路径并发修改句柄状态。
 * @post 成功时发送和接收均可显式重新启动；失败时保留 HAL 报告的状态。
 * @note 同步接口可能等待 HAL 完成中止，不要在中断上下文调用。
 */
platform_status_t platform_uart_backend_abort(
  platform_uart_instance_t uart
);

/**
 * @brief 读取 UART HAL 错误快照并转换为平台错误位。
 * @param uart UART 实例编号；应为已启用的平台枚举值。
 * @retval PLATFORM_UART_ERROR_NONE HAL 未记录已知错误。
 * @retval PLATFORM_UART_ERROR_UNKNOWN 实例非法/未映射，或 HAL 返回未识别错误位。
 * @post 只读 HAL 错误码，不清除错误，也不改变发送或接收状态。
 * @note 可在任务或中断上下文调用；与其他实例查询互不共享可变状态。
 */
uint32_t platform_uart_backend_get_error(platform_uart_instance_t uart);

/**
 * @brief 把 UART 完成、错误和中止事件分发给已注册回调。
 * @param uart UART 实例；非法或未注册时无操作。
 * @param type 事件类型；ERROR 事件附带后端当前错误位，其他事件错误位为 NONE。
 * @post 已注册且 callback 非 NULL 时同步调用一次；事件对象只在回调期间有效。
 * @warning 由 HAL 中断回调调用，执行上下文为中断；不得阻塞或分配内存。
 * @note 不保留事件或缓冲区指针；一个物理实例只有一个回调所有者。
 */
void platform_uart_dispatch_event(
  platform_uart_instance_t uart,
  platform_uart_event_type_t type
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_UART_BACKEND_H
