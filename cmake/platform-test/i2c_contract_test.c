/**
 * @file i2c_contract_test.c
 * @brief I2C Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_i2c_backend.h"
#include "platform_i2c.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static platform_status_t s_transmit_status;
static platform_status_t s_probe_status;
static uint32_t s_backend_error;
static uint32_t s_event_count;
static uint32_t s_probe_trials;

bool platform_i2c_backend_is_valid(platform_i2c_instance_t i2c) {
  return i2c == PLATFORM_I2C_1;
}

platform_status_t platform_i2c_backend_master_transmit(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)i2c;
  (void)address;
  (void)data;
  (void)size;
  (void)timeout;
  return s_transmit_status;
}

platform_status_t platform_i2c_backend_master_receive(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)i2c;
  (void)address;
  (void)data;
  (void)size;
  (void)timeout;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_i2c_backend_mem_write(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)i2c;
  (void)address;
  (void)mem_address;
  (void)mem_address_size;
  (void)data;
  (void)size;
  (void)timeout;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_i2c_backend_mem_read(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)i2c;
  (void)address;
  (void)mem_address;
  (void)mem_address_size;
  (void)data;
  (void)size;
  (void)timeout;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_i2c_backend_probe(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint32_t trials,
  uint32_t timeout
) {
  (void)i2c;
  (void)address;
  (void)timeout;
  s_probe_trials = trials;
  return s_probe_status;
}

platform_status_t platform_i2c_backend_master_transmit_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
) {
  (void)i2c;
  (void)address;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_i2c_backend_master_receive_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
) {
  (void)i2c;
  (void)address;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_i2c_backend_mem_write_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size
) {
  (void)i2c;
  (void)address;
  (void)mem_address;
  (void)mem_address_size;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_i2c_backend_mem_read_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size
) {
  (void)i2c;
  (void)address;
  (void)mem_address;
  (void)mem_address_size;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_i2c_backend_master_transmit_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
) {
  (void)i2c;
  (void)address;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_i2c_backend_master_receive_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
) {
  (void)i2c;
  (void)address;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_i2c_backend_mem_write_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size
) {
  (void)i2c;
  (void)address;
  (void)mem_address;
  (void)mem_address_size;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_i2c_backend_mem_read_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size
) {
  (void)i2c;
  (void)address;
  (void)mem_address;
  (void)mem_address_size;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_i2c_backend_abort_it(
  platform_i2c_instance_t i2c,
  uint16_t address
) {
  (void)i2c;
  (void)address;
  return PLATFORM_STATUS_OK;
}

uint32_t platform_i2c_backend_get_error(platform_i2c_instance_t i2c) {
  (void)i2c;
  return s_backend_error;
}

static void i2c_callback(
  platform_i2c_instance_t i2c,
  const platform_i2c_event_t *event,
  void *context
) {
  assert(i2c == PLATFORM_I2C_1);
  assert(event != NULL);
  assert(context == &s_event_count);
  s_event_count++;
}

static void test_i2c_contract(void) {
  uint8_t byte = 0U;
  s_transmit_status = PLATFORM_STATUS_OK;
  s_probe_status = PLATFORM_STATUS_OK;
  s_backend_error = PLATFORM_I2C_ERROR_NONE;
  s_event_count = 0U;
  s_probe_trials = 0U;

  assert(Platform_I2C_MasterTransmit(
           PLATFORM_I2C_1,
           0x80U,
           &byte,
           1U,
           10U
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_I2C_GetLastError(PLATFORM_I2C_1) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);

  assert(Platform_I2C_MemWrite(
           PLATFORM_I2C_1,
           0x50U,
           0U,
           3U,
           &byte,
           1U,
           10U
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);

  assert(Platform_I2C_Probe(
           PLATFORM_I2C_1,
           0x80U,
           1U,
           10U
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);
  assert(Platform_I2C_Probe(
           PLATFORM_I2C_1,
           0x38U,
           0U,
           10U
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);
  assert(Platform_I2C_Probe(
           PLATFORM_I2C_1,
           0x38U,
           2U,
           10U
         ) == PLATFORM_STATUS_OK);
  assert(s_probe_trials == 2U);

  s_probe_status = PLATFORM_STATUS_TIMEOUT;
  assert(Platform_I2C_Probe(
           PLATFORM_I2C_1,
           0x3DU,
           2U,
           10U
         ) == PLATFORM_STATUS_TIMEOUT);

  s_transmit_status = PLATFORM_STATUS_ERROR;
  assert(Platform_I2C_MasterTransmit(
           PLATFORM_I2C_1,
           0x50U,
           &byte,
           1U,
           10U
         ) == PLATFORM_STATUS_ERROR);
  assert((Platform_I2C_GetLastError(PLATFORM_I2C_1) &
          PLATFORM_ERROR_HAL) != 0U);

  s_transmit_status = PLATFORM_STATUS_OK;
  assert(Platform_I2C_MasterTransmit(
           PLATFORM_I2C_1,
           0x50U,
           &byte,
           1U,
           10U
         ) == PLATFORM_STATUS_OK);

  const platform_i2c_callback_binding_t callbacks = {
    .callback = i2c_callback,
    .context = &s_event_count,
  };
  assert(Platform_I2C_RegisterCallback(PLATFORM_I2C_1, &callbacks) ==
         PLATFORM_STATUS_OK);
  assert(Platform_I2C_RegisterCallback(PLATFORM_I2C_1, &callbacks) ==
         PLATFORM_STATUS_BUSY);

  s_backend_error = PLATFORM_I2C_ERROR_AF;
  platform_i2c_dispatch_event(PLATFORM_I2C_1, PLATFORM_I2C_EVENT_ERROR);
  assert(s_event_count == 1U);
}

int main(void) {
  test_i2c_contract();
  return 0;
}
