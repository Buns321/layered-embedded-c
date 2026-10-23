#include "platform_i2c_backend.h"
#include "platform_i2c.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static Platform_StatusTypeDef s_transmit_status;
static uint32_t s_backend_error;
static uint32_t s_event_count;

bool platform_i2c_backend_is_valid(Platform_I2C_Instance i2c) {
  return i2c == PLATFORM_I2C_1;
}

Platform_StatusTypeDef platform_i2c_backend_master_transmit(
  Platform_I2C_Instance i2c,
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

Platform_StatusTypeDef platform_i2c_backend_master_receive(
  Platform_I2C_Instance i2c,
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
  return Platform_OK;
}

Platform_StatusTypeDef platform_i2c_backend_mem_write(
  Platform_I2C_Instance i2c,
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
  return Platform_OK;
}

Platform_StatusTypeDef platform_i2c_backend_mem_read(
  Platform_I2C_Instance i2c,
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
  return Platform_OK;
}

Platform_StatusTypeDef platform_i2c_backend_master_transmit_it(
  Platform_I2C_Instance i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
) {
  (void)i2c;
  (void)address;
  (void)data;
  (void)size;
  return Platform_OK;
}

Platform_StatusTypeDef platform_i2c_backend_master_receive_it(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
) {
  (void)i2c;
  (void)address;
  (void)data;
  (void)size;
  return Platform_OK;
}

Platform_StatusTypeDef platform_i2c_backend_mem_write_it(
  Platform_I2C_Instance i2c,
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
  return Platform_OK;
}

Platform_StatusTypeDef platform_i2c_backend_mem_read_it(
  Platform_I2C_Instance i2c,
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
  return Platform_OK;
}

Platform_StatusTypeDef platform_i2c_backend_master_transmit_dma(
  Platform_I2C_Instance i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
) {
  (void)i2c;
  (void)address;
  (void)data;
  (void)size;
  return Platform_OK;
}

Platform_StatusTypeDef platform_i2c_backend_master_receive_dma(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
) {
  (void)i2c;
  (void)address;
  (void)data;
  (void)size;
  return Platform_OK;
}

Platform_StatusTypeDef platform_i2c_backend_mem_write_dma(
  Platform_I2C_Instance i2c,
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
  return Platform_OK;
}

Platform_StatusTypeDef platform_i2c_backend_mem_read_dma(
  Platform_I2C_Instance i2c,
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
  return Platform_OK;
}

Platform_StatusTypeDef platform_i2c_backend_abort_it(
  Platform_I2C_Instance i2c,
  uint16_t address
) {
  (void)i2c;
  (void)address;
  return Platform_OK;
}

uint32_t platform_i2c_backend_get_error(Platform_I2C_Instance i2c) {
  (void)i2c;
  return s_backend_error;
}

static void i2c_callback(
  Platform_I2C_Instance i2c,
  const Platform_I2C_Event *event,
  void *context
) {
  assert(i2c == PLATFORM_I2C_1);
  assert(event != NULL);
  assert(context == &s_event_count);
  s_event_count++;
}

static void test_i2c_contract(void) {
  uint8_t byte = 0U;
  s_transmit_status = Platform_OK;
  s_backend_error = PLATFORM_I2C_ERROR_NONE;
  s_event_count = 0U;

  assert(Platform_I2C_MasterTransmit(
           PLATFORM_I2C_1,
           0x80U,
           &byte,
           1U,
           10U
         ) == Platform_INVALID_ARGUMENT);
  assert((Platform_I2C_GetLastError(PLATFORM_I2C_1) &
          PLATFORM_COMMON_ERROR_PARAMETER) != 0U);

  assert(Platform_I2C_MemWrite(
           PLATFORM_I2C_1,
           0x50U,
           0U,
           3U,
           &byte,
           1U,
           10U
         ) == Platform_INVALID_ARGUMENT);

  s_transmit_status = Platform_ERROR;
  assert(Platform_I2C_MasterTransmit(
           PLATFORM_I2C_1,
           0x50U,
           &byte,
           1U,
           10U
         ) == Platform_ERROR);
  assert((Platform_I2C_GetLastError(PLATFORM_I2C_1) &
          PLATFORM_COMMON_ERROR_HAL) != 0U);

  s_transmit_status = Platform_OK;
  assert(Platform_I2C_MasterTransmit(
           PLATFORM_I2C_1,
           0x50U,
           &byte,
           1U,
           10U
         ) == Platform_OK);

  const platform_i2c_callbacks_t callbacks = {
    .callback = i2c_callback,
    .context = &s_event_count,
  };
  assert(Platform_I2C_RegisterCallbacks(PLATFORM_I2C_1, &callbacks) ==
         Platform_OK);
  assert(Platform_I2C_RegisterCallbacks(PLATFORM_I2C_1, &callbacks) ==
         Platform_BUSY);

  s_backend_error = PLATFORM_I2C_ERROR_AF;
  platform_i2c_dispatch_event(PLATFORM_I2C_1, PLATFORM_I2C_EVENT_ERROR);
  assert(s_event_count == 1U);
}

int main(void) {
  test_i2c_contract();
  return 0;
}
