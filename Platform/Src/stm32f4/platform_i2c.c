/**
 * @file platform_i2c.c
 * @brief STM32F4 公共 I2C 实现的 HAL 后端。
 * @ingroup platform
 */

#include "i2c.h"
#include "platform_hal_internal.h"
#include "platform_i2c_backend.h"
#include "stm32f4xx_hal.h"

static I2C_HandleTypeDef *platform_i2c_to_hal(platform_i2c_instance_t i2c) {
  switch (i2c) {
#if PLATFORM_ENABLE_I2C1
    case PLATFORM_I2C_1: return &hi2c1;
#endif
#if PLATFORM_ENABLE_I2C2
    case PLATFORM_I2C_2: return &hi2c2;
#endif
#if PLATFORM_ENABLE_I2C3
    case PLATFORM_I2C_3: return &hi2c3;
#endif
    default: return NULL;
  }
}

static bool platform_hal_to_i2c(
  I2C_HandleTypeDef *i2c_h,
  platform_i2c_instance_t *i2c
) {
  if (i2c_h == NULL || i2c == NULL) {
    return false;
  }
#if PLATFORM_ENABLE_I2C1
  if (i2c_h == &hi2c1) {
    *i2c = PLATFORM_I2C_1;
    return true;
  }
#endif
#if PLATFORM_ENABLE_I2C2
  if (i2c_h == &hi2c2) {
    *i2c = PLATFORM_I2C_2;
    return true;
  }
#endif
#if PLATFORM_ENABLE_I2C3
  if (i2c_h == &hi2c3) {
    *i2c = PLATFORM_I2C_3;
    return true;
  }
#endif
  return false;
}

static uint16_t platform_i2c_hal_address(uint16_t address) {
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

bool platform_i2c_backend_is_valid(platform_i2c_instance_t i2c) {
  return (uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT &&
         platform_i2c_to_hal(i2c) != NULL;
}

platform_status_t platform_i2c_backend_master_transmit(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Master_Transmit(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       (uint8_t *)data,
                       size,
                       timeout
                     ));
}

platform_status_t platform_i2c_backend_master_receive(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Master_Receive(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       data,
                       size,
                       timeout
                     ));
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
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Mem_Write(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       mem_address,
                       mem_address_size,
                       (uint8_t *)data,
                       size,
                       timeout
                     ));
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
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Mem_Read(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       mem_address,
                       mem_address_size,
                       data,
                       size,
                       timeout
                     ));
}

platform_status_t platform_i2c_backend_probe(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint32_t trials,
  uint32_t timeout
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_IsDeviceReady(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       trials,
                       timeout
                     ));
}

platform_status_t platform_i2c_backend_master_transmit_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Master_Transmit_IT(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       (uint8_t *)data,
                       size
                     ));
}

platform_status_t platform_i2c_backend_master_receive_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Master_Receive_IT(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       data,
                       size
                     ));
}

platform_status_t platform_i2c_backend_mem_write_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Mem_Write_IT(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       mem_address,
                       mem_address_size,
                       (uint8_t *)data,
                       size
                     ));
}

platform_status_t platform_i2c_backend_mem_read_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Mem_Read_IT(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       mem_address,
                       mem_address_size,
                       data,
                       size
                     ));
}

platform_status_t platform_i2c_backend_master_transmit_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Master_Transmit_DMA(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       (uint8_t *)data,
                       size
                     ));
}

platform_status_t platform_i2c_backend_master_receive_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Master_Receive_DMA(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       data,
                       size
                     ));
}

platform_status_t platform_i2c_backend_mem_write_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Mem_Write_DMA(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       mem_address,
                       mem_address_size,
                       (uint8_t *)data,
                       size
                     ));
}

platform_status_t platform_i2c_backend_mem_read_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Mem_Read_DMA(
                       i2c_h,
                       platform_i2c_hal_address(address),
                       mem_address,
                       mem_address_size,
                       data,
                       size
                     ));
}

platform_status_t platform_i2c_backend_abort_it(
  platform_i2c_instance_t i2c,
  uint16_t address
) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_I2C_Master_Abort_IT(
                       i2c_h,
                       platform_i2c_hal_address(address)
                     ));
}

uint32_t platform_i2c_backend_get_error(platform_i2c_instance_t i2c) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_I2C_ERROR_UNKNOWN
                   : platform_i2c_map_error(HAL_I2C_GetError(i2c_h));
}

static void platform_i2c_dispatch_hal_event(
  I2C_HandleTypeDef *i2c_h,
  platform_i2c_event_type_t type
) {
  platform_i2c_instance_t i2c;
  if (platform_hal_to_i2c(i2c_h, &i2c)) {
    platform_i2c_dispatch_event(i2c, type);
  }
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *i2c_h) {
  platform_i2c_dispatch_hal_event(
    i2c_h,
    PLATFORM_I2C_EVENT_MASTER_TX_COMPLETE
  );
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *i2c_h) {
  platform_i2c_dispatch_hal_event(
    i2c_h,
    PLATFORM_I2C_EVENT_MASTER_RX_COMPLETE
  );
}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *i2c_h) {
  platform_i2c_dispatch_hal_event(i2c_h, PLATFORM_I2C_EVENT_MEM_TX_COMPLETE);
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *i2c_h) {
  platform_i2c_dispatch_hal_event(i2c_h, PLATFORM_I2C_EVENT_MEM_RX_COMPLETE);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *i2c_h) {
  platform_i2c_dispatch_hal_event(i2c_h, PLATFORM_I2C_EVENT_ERROR);
}

void HAL_I2C_AbortCpltCallback(I2C_HandleTypeDef *i2c_h) {
  platform_i2c_dispatch_hal_event(i2c_h, PLATFORM_I2C_EVENT_ABORT_COMPLETE);
}
