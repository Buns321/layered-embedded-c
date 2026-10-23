//
// Created by Buns_ on 2026/10/23.
//

/**
 * @file platform_i2c.h
 * @brief HAL-free I2C master transport interface.
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

/** @brief Available I2C instances. */
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

/** @brief Number of possible I2C identity slots. */
#define PLATFORM_I2C_INSTANCE_COUNT 3U

/** @brief I2C error flags. */
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
 * @brief I2C completion callbacks.
 * @note Callbacks run in interrupt context.
 * @note Registration copies this structure. One physical instance has one owner.
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
 * @brief Blocking master transmit.
 * @param i2c I2C instance.
 * @param address 7-bit device address in 0x00..0x7F.
 * @param data Transmit buffer.
 * @param size Byte count.
 * @param timeout Timeout in milliseconds.
 * @pre MX_I2Cx_Init() completed successfully.
 */
Platform_StatusTypeDef Platform_I2C_MasterTransmit(
  Platform_I2C_Instance i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief Blocking master receive.
 * @param i2c I2C instance.
 * @param address 7-bit device address.
 * @param data Receive buffer.
 * @param size Byte count.
 * @param timeout Timeout in milliseconds.
 */
Platform_StatusTypeDef Platform_I2C_MasterReceive(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief Blocking memory write.
 * @param i2c I2C instance.
 * @param address 7-bit device address.
 * @param mem_address Memory address inside the device.
 * @param mem_address_size Memory-address width in bytes, normally 1 or 2.
 * @param data Transmit buffer.
 * @param size Byte count.
 * @param timeout Timeout in milliseconds.
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
 * @brief Blocking memory read.
 * @param i2c I2C instance.
 * @param address 7-bit device address.
 * @param mem_address Memory address inside the device.
 * @param mem_address_size Memory-address width in bytes, normally 1 or 2.
 * @param data Receive buffer.
 * @param size Byte count.
 * @param timeout Timeout in milliseconds.
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
 * @brief Interrupt master transmit.
 * @param i2c I2C instance.
 * @param address 7-bit device address.
 * @param data Transmit buffer.
 * @param size Byte count.
 * @note The buffer must remain valid and unchanged until completion, error, or
 *       abort. The completion callback runs in interrupt context.
 */
Platform_StatusTypeDef Platform_I2C_MasterTransmit_IT(
  Platform_I2C_Instance i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
);

/**
 * @brief Interrupt master receive.
 * @param i2c I2C instance.
 * @param address 7-bit device address.
 * @param data Receive buffer.
 * @param size Byte count.
 * @note The buffer must remain valid until completion, error, or abort. The
 *       completion callback runs in interrupt context.
 */
Platform_StatusTypeDef Platform_I2C_MasterReceive_IT(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
);

/**
 * @brief Interrupt memory write.
 * @param i2c I2C instance.
 * @param address 7-bit device address.
 * @param mem_address Memory address inside the device.
 * @param mem_address_size Memory-address width in bytes.
 * @param data Transmit buffer.
 * @param size Byte count.
 * @note The data buffer must remain valid until completion, error, or abort.
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
 * @brief Interrupt memory read.
 * @param i2c I2C instance.
 * @param address 7-bit device address.
 * @param mem_address Memory address inside the device.
 * @param mem_address_size Memory-address width in bytes.
 * @param data Receive buffer.
 * @param size Byte count.
 * @note The data buffer must remain valid until completion, error, or abort.
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
 * @brief DMA master transmit.
 * @param i2c I2C instance.
 * @param address 7-bit device address.
 * @param data Transmit buffer.
 * @param size Byte count.
 * @note The buffer must remain valid and unchanged until completion, error, or
 *       abort. The completion callback runs in interrupt context.
 */
Platform_StatusTypeDef Platform_I2C_MasterTransmit_DMA(
  Platform_I2C_Instance i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
);

/**
 * @brief DMA master receive.
 * @param i2c I2C instance.
 * @param address 7-bit device address.
 * @param data Receive buffer.
 * @param size Byte count.
 * @note The buffer must remain valid until completion, error, or abort. The
 *       completion callback runs in interrupt context.
 */
Platform_StatusTypeDef Platform_I2C_MasterReceive_DMA(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
);

/**
 * @brief DMA memory write.
 * @param i2c I2C instance.
 * @param address 7-bit device address.
 * @param mem_address Memory address inside the device.
 * @param mem_address_size Memory-address width in bytes.
 * @param data Transmit buffer.
 * @param size Byte count.
 * @note The data buffer must remain valid until completion, error, or abort.
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
 * @brief DMA memory read.
 * @param i2c I2C instance.
 * @param address 7-bit device address.
 * @param mem_address Memory address inside the device.
 * @param mem_address_size Memory-address width in bytes.
 * @param data Receive buffer.
 * @param size Byte count.
 * @note The data buffer must remain valid until completion, error, or abort.
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
 * @brief Abort the active master transfer using interrupt mode.
 * @param i2c I2C instance.
 * @param address 7-bit device address.
 * @note The abort completion callback runs in interrupt context. A new
 *       transfer may be started only after completion or error handling.
 */
Platform_StatusTypeDef Platform_I2C_Abort_IT(
  Platform_I2C_Instance i2c,
  uint16_t address
);

/**
 * @brief Read the latest HAL I2C error snapshot.
 * @param i2c I2C instance.
 * @return Platform_I2C_Error bit mask.
 */
uint32_t Platform_I2C_GetLastError(Platform_I2C_Instance i2c);

/**
 * @brief Register I2C callbacks.
 * @param i2c I2C instance.
 * @param callbacks Callback set to copy.
 * @note The structure is copied. The context lifetime must cover all uses.
 * @note Call once during initialization, before starting IT/DMA transfers.
 *       One physical instance has one callback owner for its whole lifetime.
 */
void Platform_I2C_RegisterCallbacks(
  Platform_I2C_Instance i2c,
  const platform_i2c_callbacks_t *callbacks
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_I2C_H
