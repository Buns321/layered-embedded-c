//
// 由 Buns_ 创建于 2026/10/23。
//

/**
 * @file platform_i2c.h
 * @brief 不依赖 HAL 类型的 I2C 主机传输接口。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_I2C_H
#define LAYERED_EMBEDDED_C_PLATFORM_I2C_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

#if !PLATFORM_ENABLE_I2C1 && !PLATFORM_ENABLE_I2C2 && \
    !PLATFORM_ENABLE_I2C3
#error "At least one Platform I2C instance must be enabled"
#endif

/** @brief 可用的 I2C 实例。 */
typedef enum platform_i2c_instance_e {
#if PLATFORM_ENABLE_I2C1
  PLATFORM_I2C_1 = 0U,
#endif
#if PLATFORM_ENABLE_I2C2
  PLATFORM_I2C_2 = 1U,
#endif
#if PLATFORM_ENABLE_I2C3
  PLATFORM_I2C_3 = 2U,
#endif
} platform_i2c_instance_t;

/** @brief I2C 身份槽位数量上限。 */
#define PLATFORM_I2C_INSTANCE_COUNT 3U

/** @brief I2C 错误标志。 */
typedef enum platform_i2c_error_e {
  PLATFORM_I2C_ERROR_NONE    = 0u,
  PLATFORM_I2C_ERROR_BERR    = (1u << 0),
  PLATFORM_I2C_ERROR_ARLO    = (1u << 1),
  PLATFORM_I2C_ERROR_AF      = (1u << 2),
  PLATFORM_I2C_ERROR_OVR     = (1u << 3),
  PLATFORM_I2C_ERROR_DMA     = (1u << 4),
  PLATFORM_I2C_ERROR_TIMEOUT = (1u << 5),
  PLATFORM_I2C_ERROR_SIZE    = (1u << 8),
  PLATFORM_I2C_ERROR_DMA_PARAM = (1u << 9),
  PLATFORM_I2C_ERROR_UNKNOWN = (1u << 7),
} platform_i2c_error_t;

/** @brief I2C 异步事件。 */
typedef enum platform_i2c_event_type_e {
  PLATFORM_I2C_EVENT_MASTER_TX_COMPLETE = 0U,
  PLATFORM_I2C_EVENT_MASTER_RX_COMPLETE,
  PLATFORM_I2C_EVENT_MEM_TX_COMPLETE,
  PLATFORM_I2C_EVENT_MEM_RX_COMPLETE,
  PLATFORM_I2C_EVENT_ERROR,
  PLATFORM_I2C_EVENT_ABORT_COMPLETE,
} platform_i2c_event_type_t;

/** @brief I2C 事件数据，只在回调期间有效。 */
typedef struct platform_i2c_event_s {
  platform_i2c_event_type_t type;
  uint32_t error;
} platform_i2c_event_t;

typedef void (*platform_i2c_callback_t)(
  platform_i2c_instance_t i2c,
  const platform_i2c_event_t *event,
  void *context
);

/**
 * @brief I2C 回调集合。
 * @note 回调在中断上下文执行。
 * @note 注册时会复制该结构体。一个物理实例只能有一个回调所有者。
 */
typedef struct platform_i2c_callback_binding_s {
  platform_i2c_callback_t callback;
  void *context;
} platform_i2c_callback_binding_t;

/**
 * @brief 阻塞式主机发送。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param data 发送缓冲区；不得为 NULL，调用期间只读且至少包含 size 字节。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @param timeout 等待完成或总线事件的超时时间，单位 ms；PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 整段数据发送完成并产生停止条件。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例或总线正忙。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内未完成。
 * @retval PLATFORM_STATUS_ERROR 从设备 NACK、总线错误或其他 HAL 执行失败。
 * @pre MX_I2Cx_Init() 已成功完成，且同一实例没有并发的阻塞或异步传输。
 * @post 成功时 data 内容不变；本层不保留指针，失败时总线状态由 HAL 决定。
 * @warning 阻塞期间调用任务不能继续执行，只能在任务上下文调用；data 必须覆盖整个调用。
 * @note 不分配内存，硬件会发出起始、地址、数据和停止时序。
 */
platform_status_t Platform_I2C_MasterTransmit(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief 阻塞式主机接收。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param data 接收缓冲区；不得为 NULL，调用期间可写且容量至少为 size 字节。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @param timeout 等待完成或总线事件的超时时间，单位 ms；PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 整段数据接收完成并产生停止条件。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例或总线正忙。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内未完成。
 * @retval PLATFORM_STATUS_ERROR 从设备 NACK、总线错误或其他 HAL 执行失败。
 * @pre MX_I2Cx_Init() 已成功完成，且同一实例没有并发的阻塞或异步传输。
 * @post 成功时 data 前 size 字节包含从设备数据；失败时内容可能已部分更新，不得视为有效。
 * @warning 阻塞期间调用任务不能继续执行，只能在任务上下文调用；data 必须覆盖整个调用。
 * @note 不分配内存，硬件会发出起始、地址、读取和停止时序。
 */
platform_status_t Platform_I2C_MasterReceive(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief 阻塞式存储器写。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param mem_address 器件内部存储或寄存器地址；不是 I2C 从设备地址。
 * @param mem_address_size 存储地址宽度（字节），只允许 1 或 2。
 * @param data 发送缓冲区；不得为 NULL，调用期间只读且至少包含 size 字节。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @param timeout 等待完成或总线事件的超时时间，单位 ms；PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 内部地址和整段数据发送完成并产生停止条件。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、存储地址宽度、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例或总线正忙。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内未完成。
 * @retval PLATFORM_STATUS_ERROR 从设备 NACK、总线错误或其他 HAL 执行失败。
 * @pre MX_I2Cx_Init() 已成功完成，且同一实例没有并发的阻塞或异步传输。
 * @post 成功时 data 内容不变；本层不保留指针，失败时总线状态由 HAL 决定。
 * @warning 只能在任务上下文调用；data 必须覆盖整个调用，多个任务不得并发使用同一实例。
 * @note 不分配内存；硬件先发送内部地址，再发送数据，传输序列由 HAL 实现。
 */
platform_status_t Platform_I2C_MemWrite(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief 阻塞式存储器读。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param mem_address 器件内部存储或寄存器地址；不是 I2C 从设备地址。
 * @param mem_address_size 存储地址宽度（字节），只允许 1 或 2。
 * @param data 接收缓冲区；不得为 NULL，调用期间可写且容量至少为 size 字节。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @param timeout 等待完成或总线事件的超时时间，单位 ms；PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 内部地址发送和整段数据接收完成并产生停止条件。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、存储地址宽度、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例或总线正忙。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内未完成。
 * @retval PLATFORM_STATUS_ERROR 从设备 NACK、总线错误或其他 HAL 执行失败。
 * @pre MX_I2Cx_Init() 已成功完成，且同一实例没有并发的阻塞或异步传输。
 * @post 成功时 data 前 size 字节包含从设备数据；失败时内容可能已部分更新，不得视为有效。
 * @warning 只能在任务上下文调用；data 必须覆盖整个调用，多个任务不得并发使用同一实例。
 * @note 不分配内存；硬件先写内部地址，再产生重复起始条件读取数据。
 */
platform_status_t Platform_I2C_MemRead(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief 探测 I2C 设备是否响应。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param trials 探测尝试次数，必须大于 0。
 * @param timeout 每次尝试的超时时间，单位 ms；PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @pre MX_I2Cx_Init() 已成功完成。
 * @retval PLATFORM_STATUS_OK 设备在任一尝试中响应。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址或尝试次数非法。
 * @retval PLATFORM_STATUS_TIMEOUT 设备未在超时时间内响应。
 * @retval PLATFORM_STATUS_BUSY 总线忙。
 * @retval PLATFORM_STATUS_ERROR 探测执行失败。
 * @post 只读取地址应答结果，不读写设备寄存器，也不保留任何指针。
 * @warning 只能在任务上下文调用；同一实例不得与其他传输并发。
 * @note 不分配内存；每次尝试使用独立 timeout，具体重试顺序由 HAL 决定。
 */
platform_status_t Platform_I2C_Probe(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint32_t trials,
  uint32_t timeout
);

/**
 * @brief 中断方式主机发送。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param data 发送缓冲区；不得为 NULL，至少包含 size 字节。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受中断发送请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例或总线正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或总线无法启动请求；本次请求不会产生完成事件。
 * @pre MX_I2Cx_Init() 已完成，NVIC 已配置；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受，最终结果通过完成、错误或中止事件通知。
 * @warning 在完成、错误或中止事件前，data 必须保持有效且内容不变；回调在中断上下文执行。
 * @note 异步接口没有内部超时，不分配内存；同一实例同一时刻只能有一个 IT/DMA 传输。
 */
platform_status_t Platform_I2C_MasterTransmitIT(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
);

/**
 * @brief 中断方式主机接收。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param data 接收缓冲区；不得为 NULL，容量至少为 size 字节。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受中断接收请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例或总线正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或总线无法启动请求；本次请求不会产生完成事件。
 * @pre MX_I2Cx_Init() 已完成，NVIC 已配置；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受；完成前不得读取完整 data，错误或中止时内容可能只更新一部分。
 * @warning 在完成、错误或中止事件前，data 必须保持有效且不得并发访问；回调在中断上下文执行。
 * @note 异步接口没有内部超时，不分配内存；同一实例同一时刻只能有一个 IT/DMA 传输。
 */
platform_status_t Platform_I2C_MasterReceiveIT(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
);

/**
 * @brief 中断方式存储器写。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param mem_address 器件内部存储或寄存器地址；不是 I2C 从设备地址。
 * @param mem_address_size 存储地址宽度（字节），只允许 1 或 2。
 * @param data 发送缓冲区；不得为 NULL，至少包含 size 字节。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受中断存储写请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、存储地址宽度、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例或总线正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或总线无法启动请求；本次请求不会产生完成事件。
 * @pre MX_I2Cx_Init() 已完成，NVIC 已配置；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受，最终结果通过存储写完成、错误或中止事件通知。
 * @warning 在完成、错误或中止事件前，data 必须保持有效且内容不变；回调在中断上下文执行。
 * @note 异步接口没有内部超时，不分配内存；同一实例同一时刻只能有一个 IT/DMA 传输。
 */
platform_status_t Platform_I2C_MemWriteIT(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size
);

/**
 * @brief 中断方式存储器读。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param mem_address 器件内部存储或寄存器地址；不是 I2C 从设备地址。
 * @param mem_address_size 存储地址宽度（字节），只允许 1 或 2。
 * @param data 接收缓冲区；不得为 NULL，容量至少为 size 字节。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受中断存储读请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、存储地址宽度、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例或总线正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或总线无法启动请求；本次请求不会产生完成事件。
 * @pre MX_I2Cx_Init() 已完成，NVIC 已配置；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受；完成前不得读取完整 data，错误或中止时内容可能只更新一部分。
 * @warning 在完成、错误或中止事件前，data 必须保持有效且不得并发访问；回调在中断上下文执行。
 * @note 异步接口没有内部超时，不分配内存；同一实例同一时刻只能有一个 IT/DMA 传输。
 */
platform_status_t Platform_I2C_MemReadIT(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size
);

/**
 * @brief DMA 方式主机发送。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param data 发送缓冲区；不得为 NULL，至少包含 size 字节。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受 DMA 发送请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例、总线或 DMA 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或 DMA 无法启动请求；本次请求不会产生完成事件。
 * @pre MX_I2Cx_Init() 和 CubeMX DMA 配置已完成；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受，最终结果通过完成、错误或中止事件通知。
 * @warning 在完成、错误或中止事件前，data 必须保持有效且内容不变；回调在中断上下文执行。
 * @note DMA 传输没有内部超时，不分配内存；同一实例同一时刻只能有一个 IT/DMA 传输。
 */
platform_status_t Platform_I2C_MasterTransmitDMA(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
);

/**
 * @brief DMA 方式主机接收。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param data 接收缓冲区；不得为 NULL，容量至少为 size 字节。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受 DMA 接收请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例、总线或 DMA 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或 DMA 无法启动请求；本次请求不会产生完成事件。
 * @pre MX_I2Cx_Init() 和 CubeMX DMA 配置已完成；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受；完成前不得读取完整 data，错误或中止时内容可能只更新一部分。
 * @warning 在完成、错误或中止事件前，data 必须保持有效且不得并发访问；回调在中断上下文执行。
 * @note DMA 传输没有内部超时，不分配内存；同一实例同一时刻只能有一个 IT/DMA 传输。
 */
platform_status_t Platform_I2C_MasterReceiveDMA(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
);

/**
 * @brief DMA 方式存储器写。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param mem_address 器件内部存储或寄存器地址；不是 I2C 从设备地址。
 * @param mem_address_size 存储地址宽度（字节），只允许 1 或 2。
 * @param data 发送缓冲区；不得为 NULL，至少包含 size 字节。
 * @param size 发送字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受 DMA 存储写请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、存储地址宽度、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例、总线或 DMA 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或 DMA 无法启动请求；本次请求不会产生完成事件。
 * @pre MX_I2Cx_Init() 和对应 DMA 通道已完成配置；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受，最终结果通过存储写完成、错误或中止事件通知。
 * @warning 在完成、错误或中止事件前，data 必须保持有效且内容不变；回调在中断上下文执行。
 * @note DMA 传输没有内部超时，不分配内存；同一实例同一时刻只能有一个 IT/DMA 传输。
 */
platform_status_t Platform_I2C_MemWriteDMA(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size
);

/**
 * @brief DMA 方式存储器读。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 7 位器件地址，范围 0x00..0x7F，不是左移后的 8 位 HAL 地址。
 * @param mem_address 器件内部存储或寄存器地址；不是 I2C 从设备地址。
 * @param mem_address_size 存储地址宽度（字节），只允许 1 或 2。
 * @param data 接收缓冲区；不得为 NULL，容量至少为 size 字节。
 * @param size 接收字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK HAL 已接受 DMA 存储读请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、地址、存储地址宽度、缓冲区或长度非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY 实例、总线或 DMA 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或 DMA 无法启动请求；本次请求不会产生完成事件。
 * @pre MX_I2Cx_Init() 和对应 DMA 通道已完成配置；如需事件，回调已在启动前注册。
 * @post 返回 OK 只表示请求已接受；完成前不得读取完整 data，错误或中止时内容可能只更新一部分。
 * @warning 在完成、错误或中止事件前，data 必须保持有效且不得并发访问；回调在中断上下文执行。
 * @note DMA 传输没有内部超时，不分配内存；同一实例同一时刻只能有一个 IT/DMA 传输。
 */
platform_status_t Platform_I2C_MemReadDMA(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size
);

/**
 * @brief 使用中断方式中止当前主机传输。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param address 当前传输使用的 7 位器件地址，范围 0x00..0x7F。
 * @retval PLATFORM_STATUS_OK 中止请求已接受。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或地址非法；不访问总线。
 * @retval PLATFORM_STATUS_BUSY HAL 报告实例或总线忙。
 * @retval PLATFORM_STATUS_ERROR HAL 拒绝中止或无法启动中止序列。
 * @pre 目标实例存在进行中的异步传输，且调用发生在任务上下文。
 * @post 返回 OK 只表示中止已排队；最终结果通过 ABORT_COMPLETE 事件通知。
 * @warning 在完成、错误或中止事件处理结束前，不得复用缓冲区或启动新传输；
 *          中止完成回调在中断上下文执行。
 * @note 不分配内存、不等待硬件停止；同一实例同一时刻只能有一个 IT/DMA 传输或中止序列。
 */
platform_status_t Platform_I2C_AbortIT(
  platform_i2c_instance_t i2c,
  uint16_t address
);

/**
 * @brief 读取最近一次 HAL I2C 错误快照。
 * @param i2c I2C 实例；当前仅有效枚举值可读取后端错误。
 * @retval PLATFORM_ERROR_INVALID_ARGUMENT i2c 越界。
 * @post 合法实例返回软件错误位与 HAL 错误映射的按位或；非法实例不访问快照。
 * @note 只读，不清除软件或 HAL 错误；不停止传输、不修改缓冲区，不分配内存。
 */
uint32_t Platform_I2C_GetLastError(platform_i2c_instance_t i2c);

/**
 * @brief 注册 I2C 回调。
 * @param i2c I2C 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param callbacks 待复制的回调集合；不得为 NULL，callback 成员可为 NULL 以禁用通知。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该实例已注册回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或回调集合非法。
 * @pre callbacks 指向的对象在调用期间有效；每个实例仅允许成功注册一次。
 * @post 成功时复制 callback 和 context，后续完成、错误或中止事件调用该回调。
 * @warning 回调在中断上下文执行；事件对象只在回调期间有效，禁止阻塞或分配内存。
 * @note 应在初始化阶段、启动 IT/DMA 传输前注册；context 的生命周期须覆盖所有使用过程。
 */
platform_status_t Platform_I2C_RegisterCallback(
  platform_i2c_instance_t i2c,
  const platform_i2c_callback_binding_t *callbacks
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_I2C_H
