/**
 * @file platform_i2c_common.c
 * @brief Family-independent I2C validation, errors and callback dispatch.
 */

#include "platform_error_internal.h"
#include "platform_i2c_backend.h"
#include <stddef.h>

static volatile uint32_t i2c_error[PLATFORM_I2C_INSTANCE_COUNT];
static platform_i2c_callbacks_t i2c_cb[PLATFORM_I2C_INSTANCE_COUNT];
static bool i2c_cb_registered[PLATFORM_I2C_INSTANCE_COUNT];

static Platform_StatusTypeDef platform_i2c_finish(
  Platform_I2C_Instance i2c,
  Platform_StatusTypeDef status
) {
  if (status != Platform_OK) {
    platform_error_record(
      &i2c_error[i2c],
      platform_error_from_status(status)
    );
  }
  return status;
}

static Platform_StatusTypeDef platform_i2c_invalid(
  Platform_I2C_Instance i2c
) {
  if ((uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT) {
    platform_error_record(
      &i2c_error[i2c],
      PLATFORM_COMMON_ERROR_PARAMETER
    );
  }
  return Platform_INVALID_ARGUMENT;
}

static bool platform_i2c_validate(
  Platform_I2C_Instance i2c,
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
  Platform_I2C_Instance i2c,
  Platform_I2C_EventType type
) {
  if (
    (uint32_t)i2c >= PLATFORM_I2C_INSTANCE_COUNT ||
    !i2c_cb_registered[i2c]
  ) {
    return;
  }

  Platform_I2C_Event event = {
    .type = type,
    .error = type == PLATFORM_I2C_EVENT_ERROR
               ? platform_i2c_backend_get_error(i2c)
               : PLATFORM_I2C_ERROR_NONE,
  };
  if (i2c_cb[i2c].callback != NULL) {
    i2c_cb[i2c].callback(i2c, &event, i2c_cb[i2c].context);
  }
}

Platform_StatusTypeDef Platform_I2C_MasterTransmit(
  Platform_I2C_Instance i2c,
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

Platform_StatusTypeDef Platform_I2C_MasterReceive(
  Platform_I2C_Instance i2c,
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

Platform_StatusTypeDef Platform_I2C_MemWrite(
  Platform_I2C_Instance i2c,
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

Platform_StatusTypeDef Platform_I2C_MemRead(
  Platform_I2C_Instance i2c,
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

Platform_StatusTypeDef Platform_I2C_MasterTransmit_IT(
  Platform_I2C_Instance i2c,
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

Platform_StatusTypeDef Platform_I2C_MasterReceive_IT(
  Platform_I2C_Instance i2c,
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

Platform_StatusTypeDef Platform_I2C_MemWrite_IT(
  Platform_I2C_Instance i2c,
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

Platform_StatusTypeDef Platform_I2C_MemRead_IT(
  Platform_I2C_Instance i2c,
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

Platform_StatusTypeDef Platform_I2C_MasterTransmit_DMA(
  Platform_I2C_Instance i2c,
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

Platform_StatusTypeDef Platform_I2C_MasterReceive_DMA(
  Platform_I2C_Instance i2c,
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

Platform_StatusTypeDef Platform_I2C_MemWrite_DMA(
  Platform_I2C_Instance i2c,
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

Platform_StatusTypeDef Platform_I2C_MemRead_DMA(
  Platform_I2C_Instance i2c,
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

Platform_StatusTypeDef Platform_I2C_Abort_IT(
  Platform_I2C_Instance i2c,
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

uint32_t Platform_I2C_GetLastError(Platform_I2C_Instance i2c) {
  if ((uint32_t)i2c >= PLATFORM_I2C_INSTANCE_COUNT) {
    return PLATFORM_COMMON_ERROR_PARAMETER;
  }
  return i2c_error[i2c] | platform_i2c_backend_get_error(i2c);
}

Platform_StatusTypeDef Platform_I2C_RegisterCallbacks(
  Platform_I2C_Instance i2c,
  const platform_i2c_callbacks_t *callbacks
) {
  if (
    (uint32_t)i2c >= PLATFORM_I2C_INSTANCE_COUNT ||
    callbacks == NULL ||
    !platform_i2c_backend_is_valid(i2c)
  ) {
    return Platform_INVALID_ARGUMENT;
  }
  if (i2c_cb_registered[i2c]) {
    return Platform_BUSY;
  }

  i2c_cb[i2c] = *callbacks;
  i2c_cb_registered[i2c] = true;
  return Platform_OK;
}
