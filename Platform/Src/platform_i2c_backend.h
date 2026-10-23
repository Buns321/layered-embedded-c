/**
 * @file platform_i2c_backend.h
 * @brief Private STM32-family backend for the common I2C implementation.
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_I2C_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_I2C_BACKEND_H

#include "platform_def.h"
#include "platform_i2c.h"
#include <stdbool.h>
#include <stdint.h>

bool platform_i2c_backend_is_valid(Platform_I2C_Instance i2c);

Platform_StatusTypeDef platform_i2c_backend_master_transmit(
  Platform_I2C_Instance i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

Platform_StatusTypeDef platform_i2c_backend_master_receive(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

Platform_StatusTypeDef platform_i2c_backend_mem_write(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

Platform_StatusTypeDef platform_i2c_backend_mem_read(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

Platform_StatusTypeDef platform_i2c_backend_master_transmit_it(
  Platform_I2C_Instance i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
);

Platform_StatusTypeDef platform_i2c_backend_master_receive_it(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
);

Platform_StatusTypeDef platform_i2c_backend_mem_write_it(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size
);

Platform_StatusTypeDef platform_i2c_backend_mem_read_it(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size
);

Platform_StatusTypeDef platform_i2c_backend_master_transmit_dma(
  Platform_I2C_Instance i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
);

Platform_StatusTypeDef platform_i2c_backend_master_receive_dma(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
);

Platform_StatusTypeDef platform_i2c_backend_mem_write_dma(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size
);

Platform_StatusTypeDef platform_i2c_backend_mem_read_dma(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size
);

Platform_StatusTypeDef platform_i2c_backend_abort_it(
  Platform_I2C_Instance i2c,
  uint16_t address
);

uint32_t platform_i2c_backend_get_error(Platform_I2C_Instance i2c);

void platform_i2c_dispatch_event(
  Platform_I2C_Instance i2c,
  Platform_I2C_EventType type
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_I2C_BACKEND_H
