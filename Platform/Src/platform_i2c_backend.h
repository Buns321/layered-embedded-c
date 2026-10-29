/**
 * @file platform_i2c_backend.h
 * @brief STM32 系列公共 I2C 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_I2C_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_I2C_BACKEND_H

#include "platform_def.h"
#include "platform_i2c.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 校验I2C实例是否已由板级配置启用。
 */
bool platform_i2c_backend_is_valid(platform_i2c_instance_t i2c);

/**
 * @brief 启动 I2C 主机阻塞发送。
 */
platform_status_t platform_i2c_backend_master_transmit(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

/**
 * @brief 启动 I2C 主机阻塞接收。
 */
platform_status_t platform_i2c_backend_master_receive(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
);

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
);

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
);

/**
 * @brief 探测 I2C 7 位地址是否存在应答。
 */
platform_status_t platform_i2c_backend_probe(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint32_t trials,
  uint32_t timeout
);

/**
 * @brief 启动 I2C 主机中断发送。
 */
platform_status_t platform_i2c_backend_master_transmit_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
);

/**
 * @brief 启动 I2C 主机中断接收。
 */
platform_status_t platform_i2c_backend_master_receive_it(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
);

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
);

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
);

/**
 * @brief 启动 I2C 主机 DMA 发送。
 */
platform_status_t platform_i2c_backend_master_transmit_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  const uint8_t *data,
  uint16_t size
);

/**
 * @brief 启动 I2C 主机 DMA 接收。
 */
platform_status_t platform_i2c_backend_master_receive_dma(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint8_t *data,
  uint16_t size
);

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
);

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
);

/**
 * @brief 启动I2C异步中止并映射 HAL 状态。
 */
platform_status_t platform_i2c_backend_abort_it(
  platform_i2c_instance_t i2c,
  uint16_t address
);

/**
 * @brief 返回I2C后端记录的错误位。
 */
uint32_t platform_i2c_backend_get_error(platform_i2c_instance_t i2c);

/**
 * @brief 把 I2C 完成、错误和中止事件分发给已注册回调。
 */
void platform_i2c_dispatch_event(
  platform_i2c_instance_t i2c,
  platform_i2c_event_type_t type
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_I2C_BACKEND_H
