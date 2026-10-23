/**
 * @file platform_i2c_common.c
 * @brief Family-independent I2C validation, errors and callback dispatch.
 */

#include "platform_error_internal.h"
#include "platform_i2c_backend.h"
#include <stddef.h>

static volatile uint32_t i2c_error[PLATFORM_I2C_INSTANCE_COUNT];
static platform_i2c_callback_binding_t i2c_cb[PLATFORM_I2C_INSTANCE_COUNT];
static bool i2c_cb_registered[PLATFORM_I2C_INSTANCE_COUNT];

static platform_status_t platform_i2c_finish(
  platform_i2c_instance_t i2c,
  platform_status_t status
) {
  if (status != PLATFORM_STATUS_OK) {
    platform_error_record(
      &i2c_error[i2c],
      platform_error_from_status(status)
    );
  }
  return status;
}

static platform_status_t platform_i2c_invalid(
  platform_i2c_instance_t i2c
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_record(
      &i2c_error[i2c],
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
  }
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

static bool platform_i2c_validate(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const void *data,
  uint16_t size
) {
  return (uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT &&
         platform_i2c_backend_is_valid(i2c) &&
         address <= 0x7FU &&
         data != NULL &&
         size != 0U;
}

static bool platform_i2c_mem_size_valid(uint8_t mem_address_size) {
  return mem_address_size == 1U || mem_address_size == 2U;
}

void platform_i2c_dispatch_event(
  platform_i2c_instance_t i2c,
  platform_i2c_event_type_t type
) {
  if (
    (uint32_t)i2c >= PLATFORM_I2C_INSTANCE_COUNT ||
    !i2c_cb_registered[i2c]
  ) {
    return;
  }

  platform_i2c_event_t event = {
    .type = type,
    .error = type == PLATFORM_I2C_EVENT_ERROR
               ? platform_i2c_backend_get_error(i2c)
               : PLATFORM_I2C_ERROR_NONE,
  };
  if (i2c_cb[i2c].callback != NULL) {
    i2c_cb[i2c].callback(i2c, &event, i2c_cb[i2c].context);
  }
}

platform_status_t Platform_I2C_MasterTransmit(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (!platform_i2c_validate(i2c, address, data, size)) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_master_transmit(
      i2c,
      address,
      data,
      size,
      timeout
    )
  );
}

platform_status_t Platform_I2C_MasterReceive(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (!platform_i2c_validate(i2c, address, data, size)) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_master_receive(
      i2c,
      address,
      data,
      size,
      timeout
    )
  );
}

platform_status_t Platform_I2C_MemWrite(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (
    !platform_i2c_validate(i2c, address, data, size) ||
    !platform_i2c_mem_size_valid(mem_address_size)
  ) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_mem_write(
      i2c,
      address,
      mem_address,
      mem_address_size,
      data,
      size,
      timeout
    )
  );
}

platform_status_t Platform_I2C_MemRead(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (
    !platform_i2c_validate(i2c, address, data, size) ||
    !platform_i2c_mem_size_valid(mem_address_size)
  ) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_mem_read(
      i2c,
      address,
      mem_address,
      mem_address_size,
      data,
      size,
      timeout
    )
  );
}

platform_status_t Platform_I2C_MasterTransmitIT(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (!platform_i2c_validate(i2c, address, data, size)) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_master_transmit_it(i2c, address, data, size)
  );
}

platform_status_t Platform_I2C_MasterReceiveIT(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (!platform_i2c_validate(i2c, address, data, size)) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_master_receive_it(i2c, address, data, size)
  );
}

platform_status_t Platform_I2C_MemWriteIT(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (
    !platform_i2c_validate(i2c, address, data, size) ||
    !platform_i2c_mem_size_valid(mem_address_size)
  ) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_mem_write_it(
      i2c,
      address,
      mem_address,
      mem_address_size,
      data,
      size
    )
  );
}

platform_status_t Platform_I2C_MemReadIT(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (
    !platform_i2c_validate(i2c, address, data, size) ||
    !platform_i2c_mem_size_valid(mem_address_size)
  ) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_mem_read_it(
      i2c,
      address,
      mem_address,
      mem_address_size,
      data,
      size
    )
  );
}

platform_status_t Platform_I2C_MasterTransmitDMA(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (!platform_i2c_validate(i2c, address, data, size)) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_master_transmit_dma(i2c, address, data, size)
  );
}

platform_status_t Platform_I2C_MasterReceiveDMA(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (!platform_i2c_validate(i2c, address, data, size)) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_master_receive_dma(i2c, address, data, size)
  );
}

platform_status_t Platform_I2C_MemWriteDMA(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (
    !platform_i2c_validate(i2c, address, data, size) ||
    !platform_i2c_mem_size_valid(mem_address_size)
  ) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_mem_write_dma(
      i2c,
      address,
      mem_address,
      mem_address_size,
      data,
      size
    )
  );
}

platform_status_t Platform_I2C_MemReadDMA(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (
    !platform_i2c_validate(i2c, address, data, size) ||
    !platform_i2c_mem_size_valid(mem_address_size)
  ) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_mem_read_dma(
      i2c,
      address,
      mem_address,
      mem_address_size,
      data,
      size
    )
  );
}

platform_status_t Platform_I2C_AbortIT(
  platform_i2c_instance_t i2c,
  uint16_t address
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_reset(&i2c_error[i2c]);
  }
  if (
    (uint32_t)i2c >= PLATFORM_I2C_INSTANCE_COUNT ||
    !platform_i2c_backend_is_valid(i2c) ||
    address > 0x7FU
  ) {
    return platform_i2c_invalid(i2c);
  }
  return platform_i2c_finish(
    i2c,
    platform_i2c_backend_abort_it(i2c, address)
  );
}

uint32_t Platform_I2C_GetLastError(platform_i2c_instance_t i2c) {
  if ((uint32_t)i2c >= PLATFORM_I2C_INSTANCE_COUNT) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return i2c_error[i2c] | platform_i2c_backend_get_error(i2c);
}

platform_status_t Platform_I2C_RegisterCallback(
  platform_i2c_instance_t i2c,
  const platform_i2c_callback_binding_t *callbacks
) {
  if (
    (uint32_t)i2c >= PLATFORM_I2C_INSTANCE_COUNT ||
    callbacks == NULL ||
    !platform_i2c_backend_is_valid(i2c)
  ) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (i2c_cb_registered[i2c]) {
    return PLATFORM_STATUS_BUSY;
  }

  i2c_cb[i2c] = *callbacks;
  i2c_cb_registered[i2c] = true;
  return PLATFORM_STATUS_OK;
}
