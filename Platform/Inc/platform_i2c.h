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
typedef enum {
#if PLATFORM_ENABLE_I2C1
  PLATFORM_I2C_1 = 0U,
#endif
#if PLATFORM_ENABLE_I2C2
  PLATFORM_I2C_2 = 1U,
#endif
#if PLATFORM_ENABLE_I2C3
  PLATFORM_I2C_3 = 2U,
#endif
} Platform_I2C_Instance;

/** @brief I2C 身份槽位数量上限。 */
#define PLATFORM_I2C_INSTANCE_COUNT 3U

/** @brief I2C 错误标志。 */
typedef enum {
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
} Platform_I2C_Error;

typedef void (*Platform_I2C_Callback)(
  Platform_I2C_Instance i2c,
  void *context
);

/**
 * @brief I2C 完成回调集合。
 * @note 回调在中断上下文执行。
 * @note 注册时会复制该结构体。一个物理实例只能有一个回调所有者。
 */
typedef struct {
  Platform_I2C_Callback master_tx_cplt_cb;
  Platform_I2C_Callback master_rx_cplt_cb;
  Platform_I2C_Callback mem_tx_cplt_cb;
  Platform_I2C_Callback mem_rx_cplt_cb;
  Platform_I2C_Callback err_cb;
  Platform_I2C_Callback abort_cplt_cb;
  void *context;
} platform_i2c_callbacks_t;

/**
 * @brief 阻塞式主机发送。
 * @param i2c I2C 实例。
 * @param address 7 位器件地址，范围 0x00..0x7F。
 * @param data 发送缓冲区。
 * @param size 字节数。
 * @param timeout 超时时间（ms）。
 * @pre MX_I2Cx_Init() 已成功完成。
 */
Platform_StatusTypeDef Platform_I2C_MasterTransmit(
  Platform_I2C_Instance i2c,
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
Platform_StatusTypeDef Platform_I2C_MasterReceive(
  Platform_I2C_Instance i2c,
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
Platform_StatusTypeDef Platform_I2C_MemWrite(
  Platform_I2C_Instance i2c,
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
Platform_StatusTypeDef Platform_I2C_MemRead(
  Platform_I2C_Instance i2c,
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
Platform_StatusTypeDef Platform_I2C_MasterTransmit_IT(
  Platform_I2C_Instance i2c,
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
Platform_StatusTypeDef Platform_I2C_MasterReceive_IT(
  Platform_I2C_Instance i2c,
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
Platform_StatusTypeDef Platform_I2C_MemWrite_IT(
  Platform_I2C_Instance i2c,
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
Platform_StatusTypeDef Platform_I2C_MemRead_IT(
  Platform_I2C_Instance i2c,
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
Platform_StatusTypeDef Platform_I2C_MasterTransmit_DMA(
  Platform_I2C_Instance i2c,
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
Platform_StatusTypeDef Platform_I2C_MasterReceive_DMA(
  Platform_I2C_Instance i2c,
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
Platform_StatusTypeDef Platform_I2C_MemWrite_DMA(
  Platform_I2C_Instance i2c,
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
Platform_StatusTypeDef Platform_I2C_MemRead_DMA(
  Platform_I2C_Instance i2c,
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
Platform_StatusTypeDef Platform_I2C_Abort_IT(
  Platform_I2C_Instance i2c,
  uint16_t address
);

/**
 * @brief 读取最近一次 HAL I2C 错误快照。
 * @param i2c I2C 实例。
 * @return Platform_I2C_Error 位掩码。
 */
uint32_t Platform_I2C_GetLastError(Platform_I2C_Instance i2c);

/**
 * @brief 注册 I2C 回调。
 * @param i2c I2C 实例。
 * @param callbacks 待复制的回调集合。
 * @note 结构体会被复制。context 的生命周期必须覆盖所有使用过程。
 * @note 应在初始化阶段、启动 IT/DMA 传输前调用一次。一个物理实例在整个
 *       生命周期内只能有一个回调所有者。
 */
void Platform_I2C_RegisterCallbacks(
  Platform_I2C_Instance i2c,
  const platform_i2c_callbacks_t *callbacks
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_I2C_H
