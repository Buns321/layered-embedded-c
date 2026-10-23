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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址，范围 0x00..0x7F。
 * @param data 发送缓冲区。
 * @param size 字节数。
 * @param timeout 超时时间（ms）。
 * @pre MX_I2Cx_Init() 已成功完成。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param data 接收缓冲区。
 * @param size 字节数。
 * @param timeout 超时时间（ms）。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param mem_address 器件内部存储地址。
 * @param mem_address_size 存储地址宽度（字节），通常为 1 或 2。
 * @param data 发送缓冲区。
 * @param size 字节数。
 * @param timeout 超时时间（ms）。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param mem_address 器件内部存储地址。
 * @param mem_address_size 存储地址宽度（字节），通常为 1 或 2。
 * @param data 接收缓冲区。
 * @param size 字节数。
 * @param timeout 超时时间（ms）。
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
 * @brief 中断方式主机发送。
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param data 发送缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，缓冲区必须保持有效且内容不变。完成回调
 *       在中断上下文执行。
 */
platform_status_t Platform_I2C_MasterTransmitIT(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
);

/**
 * @brief 中断方式主机接收。
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param data 接收缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，缓冲区必须保持有效。完成回调在中断上下文
 *       执行。
 */
platform_status_t Platform_I2C_MasterReceiveIT(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
);

/**
 * @brief 中断方式存储器写。
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param mem_address 器件内部存储地址。
 * @param mem_address_size 存储地址宽度（字节）。
 * @param data 发送缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，数据缓冲区必须保持有效。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param mem_address 器件内部存储地址。
 * @param mem_address_size 存储地址宽度（字节）。
 * @param data 接收缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，数据缓冲区必须保持有效。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param data 发送缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，缓冲区必须保持有效且内容不变。完成回调
 *       在中断上下文执行。
 */
platform_status_t Platform_I2C_MasterTransmitDMA(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
);

/**
 * @brief DMA 方式主机接收。
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param data 接收缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，缓冲区必须保持有效。完成回调在中断上下文
 *       执行。
 */
platform_status_t Platform_I2C_MasterReceiveDMA(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
);

/**
 * @brief DMA 方式存储器写。
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param mem_address 器件内部存储地址。
 * @param mem_address_size 存储地址宽度（字节）。
 * @param data 发送缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，数据缓冲区必须保持有效。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @param mem_address 器件内部存储地址。
 * @param mem_address_size 存储地址宽度（字节）。
 * @param data 接收缓冲区。
 * @param size 字节数。
 * @note 在完成、错误或中止前，数据缓冲区必须保持有效。
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
 * @param i2c I2C 实例。
 * @param address 7 位器件地址。
 * @note 中止完成回调在中断上下文执行。只有在完成或错误处理结束后，才能
 *       启动新的传输。
 */
platform_status_t Platform_I2C_AbortIT(
  platform_i2c_instance_t i2c,
  uint16_t address
);

/**
 * @brief 读取最近一次 HAL I2C 错误快照。
 * @param i2c I2C 实例。
 * @return platform_i2c_error_t 位掩码。
 */
uint32_t Platform_I2C_GetLastError(platform_i2c_instance_t i2c);

/**
 * @brief 注册 I2C 回调。
 * @param i2c I2C 实例。
 * @param callbacks 待复制的回调集合。
 * @note 结构体会被复制。context 的生命周期必须覆盖所有使用过程。
 * @note 应在初始化阶段、启动 IT/DMA 传输前调用一次。一个物理实例在整个
 *       生命周期内只能有一个回调所有者。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该实例已注册回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或回调集合非法。
 */
platform_status_t Platform_I2C_RegisterCallback(
  platform_i2c_instance_t i2c,
  const platform_i2c_callback_binding_t *callbacks
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_I2C_H
