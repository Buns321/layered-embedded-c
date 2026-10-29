/**
 * @file platform_i2c.c
 * @brief STM32F4 公共 I2C 实现的 HAL 后端。
 * @ingroup platform
 */

#include "i2c.h"
#include "platform_hal_internal.h"
#include "platform_i2c_backend.h"
#include "stm32f4xx_hal.h"

/**
 * @brief 把平台 I2C 实例编号转换为 HAL 句柄。
 */
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

/**
 * @brief 把 HAL I2C 句柄识别为平台实例编号。
 */
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

/**
 * @brief 把 7 位地址转换为 HAL 期望格式。
 */
static uint16_t platform_i2c_hal_address(uint16_t address) {
  return (uint16_t)(address << 1U);
}

/**
 * @brief 把 HAL I2C 错误位映射为平台错误码。
 */
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

/**
 * @brief 校验I2C实例是否已由板级配置启用。
 */
bool platform_i2c_backend_is_valid(platform_i2c_instance_t i2c) {
  return (uint32_t)i2c < PLATFORM_I2C_INSTANCE_COUNT &&
         platform_i2c_to_hal(i2c) != NULL;
}

/**
 * @brief 启动 I2C 主机阻塞发送。
 */
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

/**
 * @brief 启动 I2C 主机阻塞接收。
 */
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

/**
 * @brief 启动 I2C 寄存器阻塞写入。
 */
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

/**
 * @brief 启动 I2C 寄存器阻塞读取。
 */
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

/**
 * @brief 探测 I2C 7 位地址是否存在应答。
 */
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

/**
 * @brief 启动 I2C 主机中断发送。
 */
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

/**
 * @brief 启动 I2C 主机中断接收。
 */
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

/**
 * @brief 启动 I2C 寄存器中断写入。
 */
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

/**
 * @brief 启动 I2C 寄存器中断读取。
 */
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

/**
 * @brief 启动 I2C 主机 DMA 发送。
 */
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

/**
 * @brief 启动 I2C 主机 DMA 接收。
 */
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

/**
 * @brief 启动 I2C 寄存器 DMA 写入。
 */
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

/**
 * @brief 启动 I2C 寄存器 DMA 读取。
 */
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

/**
 * @brief 启动I2C异步中止并映射 HAL 状态。
 */
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

/**
 * @brief 返回I2C后端记录的错误位。
 */
uint32_t platform_i2c_backend_get_error(platform_i2c_instance_t i2c) {
  I2C_HandleTypeDef *i2c_h = platform_i2c_to_hal(i2c);
  return i2c_h == NULL ? PLATFORM_I2C_ERROR_UNKNOWN
                   : platform_i2c_map_error(HAL_I2C_GetError(i2c_h));
}

/**
 * @brief 把 HAL I2C 回调事件转发给已注册的项目回调。
 */
static void platform_i2c_dispatch_hal_event(
  I2C_HandleTypeDef *i2c_h,
  platform_i2c_event_type_t type
) {
  platform_i2c_instance_t i2c;
  if (platform_hal_to_i2c(i2c_h, &i2c)) {
    platform_i2c_dispatch_event(i2c, type);
  }
}

/**
 * @brief 分发 I2C 主机发送完成事件。
 */
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *i2c_h) {
  platform_i2c_dispatch_hal_event(
    i2c_h,
    PLATFORM_I2C_EVENT_MASTER_TX_COMPLETE
  );
}

/**
 * @brief 分发 I2C 主机接收完成事件。
 */
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *i2c_h) {
  platform_i2c_dispatch_hal_event(
    i2c_h,
    PLATFORM_I2C_EVENT_MASTER_RX_COMPLETE
  );
}

/**
 * @brief 分发 I2C 寄存器写入完成事件。
 */
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *i2c_h) {
  platform_i2c_dispatch_hal_event(i2c_h, PLATFORM_I2C_EVENT_MEM_TX_COMPLETE);
}

/**
 * @brief 分发 I2C 寄存器读取完成事件。
 */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *i2c_h) {
  platform_i2c_dispatch_hal_event(i2c_h, PLATFORM_I2C_EVENT_MEM_RX_COMPLETE);
}

/**
 * @brief 记录并分发 I2C 错误事件。
 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *i2c_h) {
  platform_i2c_dispatch_hal_event(i2c_h, PLATFORM_I2C_EVENT_ERROR);
}

/**
 * @brief 分发 I2C 中止完成事件。
 */
void HAL_I2C_AbortCpltCallback(I2C_HandleTypeDef *i2c_h) {
  platform_i2c_dispatch_hal_event(i2c_h, PLATFORM_I2C_EVENT_ABORT_COMPLETE);
}
