//
// Created by Buns_ on 2026/10/23.
//

#include "platform_i2c.h"
#include "platform_hal_internal.h"
#include "i2c.h"
#include "stm32f1xx_hal.h"
#include <assert.h>
#include <stdbool.h>

static platform_i2c_callbacks_t i2c_cb[PLATFORM_I2C_INSTANCE_COUNT];
static bool i2c_cb_registered[PLATFORM_I2C_INSTANCE_COUNT];

static I2C_HandleTypeDef *platform_i2c_to_hal(Platform_I2C_Instance i2c) {
  switch (i2c) {
#if PLATFORM_ENABLE_I2C1
    case PLATFORM_I2C_1: return &hi2c1;
#endif
#if PLATFORM_ENABLE_I2C2
    case PLATFORM_I2C_2: return &hi2c2;
#endif
    default: assert(0); return NULL;
  }
}

static bool platform_i2c_from_hal(
  I2C_HandleTypeDef *hi2c,
  Platform_I2C_Instance *i2c
) {
  assert(hi2c != NULL && i2c != NULL);

#if PLATFORM_ENABLE_I2C1
  if (hi2c == &hi2c1) {
    *i2c = PLATFORM_I2C_1;
    return true;
  }
#endif
#if PLATFORM_ENABLE_I2C2
  if (hi2c == &hi2c2) {
    *i2c = PLATFORM_I2C_2;
    return true;
  }
#endif
  return false;
}

static uint16_t platform_i2c_hal_address(uint16_t address) {
  assert(address <= 0x7FU);
  return (uint16_t)(address << 1U);
}

static uint32_t platform_i2c_map_error(uint32_t hal_error) {
  uint32_t error = PLATFORM_I2C_ERROR_NONE;

  if ((hal_error & HAL_I2C_ERROR_BERR) != 0U) {
    error |= PLATFORM_I2C_ERROR_BERR;
  }
  if ((hal_error & HAL_I2C_ERROR_ARLO) != 0U) {
    error |= PLATFORM_I2C_ERROR_ARLO;
  }
  if ((hal_error & HAL_I2C_ERROR_AF) != 0U) {
    error |= PLATFORM_I2C_ERROR_AF;
  }
  if ((hal_error & HAL_I2C_ERROR_OVR) != 0U) {
    error |= PLATFORM_I2C_ERROR_OVR;
  }
  if ((hal_error & HAL_I2C_ERROR_DMA) != 0U) {
    error |= PLATFORM_I2C_ERROR_DMA;
  }
  if ((hal_error & HAL_I2C_ERROR_TIMEOUT) != 0U) {
    error |= PLATFORM_I2C_ERROR_TIMEOUT;
  }
#ifdef HAL_I2C_ERROR_SIZE
  if ((hal_error & HAL_I2C_ERROR_SIZE) != 0U) {
    error |= PLATFORM_I2C_ERROR_SIZE;
  }
#endif
#ifdef HAL_I2C_ERROR_DMA_PARAM
  if ((hal_error & HAL_I2C_ERROR_DMA_PARAM) != 0U) {
    error |= PLATFORM_I2C_ERROR_DMA_PARAM;
  }
#endif

  uint32_t known_errors =
    HAL_I2C_ERROR_BERR |
    HAL_I2C_ERROR_ARLO |
    HAL_I2C_ERROR_AF |
    HAL_I2C_ERROR_OVR |
    HAL_I2C_ERROR_DMA |
    HAL_I2C_ERROR_TIMEOUT;
#ifdef HAL_I2C_ERROR_SIZE
  known_errors |= HAL_I2C_ERROR_SIZE;
#endif
#ifdef HAL_I2C_ERROR_DMA_PARAM
  known_errors |= HAL_I2C_ERROR_DMA_PARAM;
#endif

  if ((hal_error & ~known_errors) != 0U) {
    error |= PLATFORM_I2C_ERROR_UNKNOWN;
  }
  return error;
}

static void platform_i2c_validate(
  Platform_I2C_Instance i2c,
  uint16_t address,
  const void *data,
  uint16_t size
) {
  assert(
    (uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT &&
    address <= 0x7FU &&
    data != NULL &&
    size != 0U
  );
  (void)i2c;
  (void)address;
  (void)data;
  (void)size;
}

static void platform_i2c_validate_mem(uint8_t mem_address_size) {
  assert(mem_address_size == 1U || mem_address_size == 2U);
  (void)mem_address_size;
}

Platform_StatusTypeDef Platform_I2C_MasterTransmit(
  Platform_I2C_Instance i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  platform_i2c_validate(i2c, address, data, size);
  I2C_HandleTypeDef *hi2c = platform_i2c_to_hal(i2c);
  assert(hi2c != NULL);
  return platform_from_hal_status(HAL_I2C_Master_Transmit(
    hi2c,
    platform_i2c_hal_address(address),
    (uint8_t *)data,
    size,
    timeout
  ));
}

Platform_StatusTypeDef Platform_I2C_MasterReceive(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  platform_i2c_validate(i2c, address, data, size);
  I2C_HandleTypeDef *hi2c = platform_i2c_to_hal(i2c);
  assert(hi2c != NULL);
  return platform_from_hal_status(HAL_I2C_Master_Receive(
    hi2c,
    platform_i2c_hal_address(address),
    data,
    size,
    timeout
  ));
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
  platform_i2c_validate(i2c, address, data, size);
  platform_i2c_validate_mem(mem_address_size);
  I2C_HandleTypeDef *hi2c = platform_i2c_to_hal(i2c);
  assert(hi2c != NULL);
  return platform_from_hal_status(HAL_I2C_Mem_Write(
    hi2c,
    platform_i2c_hal_address(address),
    mem_address,
    mem_address_size,
    (uint8_t *)data,
    size,
    timeout
  ));
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
  platform_i2c_validate(i2c, address, data, size);
  platform_i2c_validate_mem(mem_address_size);
  I2C_HandleTypeDef *hi2c = platform_i2c_to_hal(i2c);
  assert(hi2c != NULL);
  return platform_from_hal_status(HAL_I2C_Mem_Read(
    hi2c,
    platform_i2c_hal_address(address),
    mem_address,
    mem_address_size,
    data,
    size,
    timeout
  ));
}

Platform_StatusTypeDef Platform_I2C_MasterTransmit_IT(
  Platform_I2C_Instance i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
) {
  platform_i2c_validate(i2c, address, data, size);
  I2C_HandleTypeDef *hi2c = platform_i2c_to_hal(i2c);
  assert(hi2c != NULL);
  return platform_from_hal_status(HAL_I2C_Master_Transmit_IT(
    hi2c,
    platform_i2c_hal_address(address),
    (uint8_t *)data,
    size
  ));
}

Platform_StatusTypeDef Platform_I2C_MasterReceive_IT(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
) {
  platform_i2c_validate(i2c, address, data, size);
  I2C_HandleTypeDef *hi2c = platform_i2c_to_hal(i2c);
  assert(hi2c != NULL);
  return platform_from_hal_status(HAL_I2C_Master_Receive_IT(
    hi2c,
    platform_i2c_hal_address(address),
    data,
    size
  ));
}

Platform_StatusTypeDef Platform_I2C_MemWrite_IT(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size
) {
  platform_i2c_validate(i2c, address, data, size);
  platform_i2c_validate_mem(mem_address_size);
  I2C_HandleTypeDef *hi2c = platform_i2c_to_hal(i2c);
  assert(hi2c != NULL);
  return platform_from_hal_status(HAL_I2C_Mem_Write_IT(
    hi2c,
    platform_i2c_hal_address(address),
    mem_address,
    mem_address_size,
    (uint8_t *)data,
    size
  ));
}

Platform_StatusTypeDef Platform_I2C_MemRead_IT(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size
) {
  platform_i2c_validate(i2c, address, data, size);
  platform_i2c_validate_mem(mem_address_size);
  I2C_HandleTypeDef *hi2c = platform_i2c_to_hal(i2c);
  assert(hi2c != NULL);
  return platform_from_hal_status(HAL_I2C_Mem_Read_IT(
    hi2c,
    platform_i2c_hal_address(address),
    mem_address,
    mem_address_size,
    data,
    size
  ));
}

Platform_StatusTypeDef Platform_I2C_MasterTransmit_DMA(
  Platform_I2C_Instance i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
) {
  platform_i2c_validate(i2c, address, data, size);
  I2C_HandleTypeDef *hi2c = platform_i2c_to_hal(i2c);
  assert(hi2c != NULL);
  return platform_from_hal_status(HAL_I2C_Master_Transmit_DMA(
    hi2c,
    platform_i2c_hal_address(address),
    (uint8_t *)data,
    size
  ));
}

Platform_StatusTypeDef Platform_I2C_MasterReceive_DMA(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
) {
  platform_i2c_validate(i2c, address, data, size);
  I2C_HandleTypeDef *hi2c = platform_i2c_to_hal(i2c);
  assert(hi2c != NULL);
  return platform_from_hal_status(HAL_I2C_Master_Receive_DMA(
    hi2c,
    platform_i2c_hal_address(address),
    data,
    size
  ));
}

Platform_StatusTypeDef Platform_I2C_MemWrite_DMA(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size
) {
  platform_i2c_validate(i2c, address, data, size);
  platform_i2c_validate_mem(mem_address_size);
  I2C_HandleTypeDef *hi2c = platform_i2c_to_hal(i2c);
  assert(hi2c != NULL);
  return platform_from_hal_status(HAL_I2C_Mem_Write_DMA(
    hi2c,
    platform_i2c_hal_address(address),
    mem_address,
    mem_address_size,
    (uint8_t *)data,
    size
  ));
}

Platform_StatusTypeDef Platform_I2C_MemRead_DMA(
  Platform_I2C_Instance i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size
) {
  platform_i2c_validate(i2c, address, data, size);
  platform_i2c_validate_mem(mem_address_size);
  I2C_HandleTypeDef *hi2c = platform_i2c_to_hal(i2c);
  assert(hi2c != NULL);
  return platform_from_hal_status(HAL_I2C_Mem_Read_DMA(
    hi2c,
    platform_i2c_hal_address(address),
    mem_address,
    mem_address_size,
    data,
    size
  ));
}

Platform_StatusTypeDef Platform_I2C_Abort_IT(
  Platform_I2C_Instance i2c,
  uint16_t address
) {
  assert(address <= 0x7FU);
  I2C_HandleTypeDef *hi2c = platform_i2c_to_hal(i2c);
  assert(hi2c != NULL);
  return platform_from_hal_status(HAL_I2C_Master_Abort_IT(
    hi2c,
    platform_i2c_hal_address(address)
  ));
}

uint32_t Platform_I2C_GetLastError(Platform_I2C_Instance i2c) {
  I2C_HandleTypeDef *hi2c = platform_i2c_to_hal(i2c);
  assert(hi2c != NULL);
  return platform_i2c_map_error(HAL_I2C_GetError(hi2c));
}

void Platform_I2C_RegisterCallbacks(
  Platform_I2C_Instance i2c,
  const platform_i2c_callbacks_t *callbacks
) {
  assert(
    (uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT &&
    callbacks != NULL &&
    !i2c_cb_registered[i2c] &&
    platform_i2c_to_hal(i2c) != NULL
  );
  i2c_cb[i2c] = *callbacks;
  i2c_cb_registered[i2c] = true;
}

typedef enum {
  PLATFORM_I2C_EVENT_MASTER_TX_CPLT = 0,
  PLATFORM_I2C_EVENT_MASTER_RX_CPLT,
  PLATFORM_I2C_EVENT_MEM_TX_CPLT,
  PLATFORM_I2C_EVENT_MEM_RX_CPLT,
  PLATFORM_I2C_EVENT_ERROR,
  PLATFORM_I2C_EVENT_ABORT_CPLT,
} platform_i2c_event_t;

static void platform_i2c_dispatch(
  I2C_HandleTypeDef *hi2c,
  platform_i2c_event_t event
) {
  Platform_I2C_Instance i2c;
  if (!platform_i2c_from_hal(hi2c, &i2c)) {
    return;
  }

  Platform_I2C_Callback callback = NULL;
  switch (event) {
    case PLATFORM_I2C_EVENT_MASTER_TX_CPLT:
      callback = i2c_cb[i2c].master_tx_cplt_cb;
      break;
    case PLATFORM_I2C_EVENT_MASTER_RX_CPLT:
      callback = i2c_cb[i2c].master_rx_cplt_cb;
      break;
    case PLATFORM_I2C_EVENT_MEM_TX_CPLT:
      callback = i2c_cb[i2c].mem_tx_cplt_cb;
      break;
    case PLATFORM_I2C_EVENT_MEM_RX_CPLT:
      callback = i2c_cb[i2c].mem_rx_cplt_cb;
      break;
    case PLATFORM_I2C_EVENT_ERROR:
      callback = i2c_cb[i2c].err_cb;
      break;
    case PLATFORM_I2C_EVENT_ABORT_CPLT:
      callback = i2c_cb[i2c].abort_cplt_cb;
      break;
    default:
      assert(0);
      break;
  }

  if (callback != NULL) {
    callback(i2c, i2c_cb[i2c].context);
  }
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c) {
  platform_i2c_dispatch(hi2c, PLATFORM_I2C_EVENT_MASTER_TX_CPLT);
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  platform_i2c_dispatch(hi2c, PLATFORM_I2C_EVENT_MASTER_RX_CPLT);
}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
  platform_i2c_dispatch(hi2c, PLATFORM_I2C_EVENT_MEM_TX_CPLT);
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  platform_i2c_dispatch(hi2c, PLATFORM_I2C_EVENT_MEM_RX_CPLT);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
  platform_i2c_dispatch(hi2c, PLATFORM_I2C_EVENT_ERROR);
}

void HAL_I2C_AbortCpltCallback(I2C_HandleTypeDef *hi2c) {
  platform_i2c_dispatch(hi2c, PLATFORM_I2C_EVENT_ABORT_CPLT);
}
