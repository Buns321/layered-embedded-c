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

bool platform_i2c_backend_is_valid(platform_i2c_instance_t i2c);

platform_status_t platform_i2c_backend_master_transmit(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

platform_status_t platform_i2c_backend_master_receive(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

platform_status_t platform_i2c_backend_mem_write(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

platform_status_t platform_i2c_backend_mem_read(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

platform_status_t platform_i2c_backend_master_transmit_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
);

platform_status_t platform_i2c_backend_master_receive_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
);

platform_status_t platform_i2c_backend_mem_write_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size
);

platform_status_t platform_i2c_backend_mem_read_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size
);

platform_status_t platform_i2c_backend_master_transmit_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
);

platform_status_t platform_i2c_backend_master_receive_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
);

platform_status_t platform_i2c_backend_mem_write_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size
);

platform_status_t platform_i2c_backend_mem_read_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size
);

platform_status_t platform_i2c_backend_abort_it(
  platform_i2c_instance_t i2c,
  uint16_t address
);

uint32_t platform_i2c_backend_get_error(platform_i2c_instance_t i2c);

void platform_i2c_dispatch_event(
  platform_i2c_instance_t i2c,
  platform_i2c_event_type_t type
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_I2C_BACKEND_H
