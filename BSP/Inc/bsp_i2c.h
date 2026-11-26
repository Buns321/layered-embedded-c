/**
 * @file bsp_i2c.h
 * @brief 轻量主机 I2C 探测与寄存器读写接口（按 HAL_I2C_MODULE_ENABLED 启用）。
 * @addtogroup bsp
 * @{
 */

#ifndef LAYERED_CAR_BSP_I2C_H
#define LAYERED_CAR_BSP_I2C_H

#include "bsp_def.h"
#include "bsp_mcu.h"
#include <stdint.h>

#if defined(HAL_I2C_MODULE_ENABLED) || defined(BSP_HOST_STUB)

/**
 * @brief I2C 从设备描述符；addr_7bit 为 7 位地址（0x00..0x7F），内部自动左移 1 位。
 */
typedef struct bsp_i2c_s {
  I2C_HandleTypeDef *hi2c;
  uint8_t addr_7bit;   /* 0x00..0x7F */
  uint32_t timeout_ms; /* 阻塞超时，单位 ms，必须 > 0 */
} bsp_i2c_t;

/**
 * @brief 探测从设备是否应答；仅任务上下文阻塞调用。
 */
bsp_status_t BSP_I2C_Probe(const bsp_i2c_t *dev, uint32_t trials);

/**
 * @brief 阻塞写从设备寄存器（reg_size 为 1 或 2 byte）。
 */
bsp_status_t BSP_I2C_MemWrite(const bsp_i2c_t *dev, uint16_t reg, uint16_t reg_size,
                              const uint8_t *data, uint16_t len);

/**
 * @brief 阻塞读从设备寄存器（reg_size 为 1 或 2 byte）。
 */
bsp_status_t BSP_I2C_MemRead(const bsp_i2c_t *dev, uint16_t reg, uint16_t reg_size, uint8_t *data,
                             uint16_t len);

#endif /* HAL_I2C_MODULE_ENABLED || BSP_HOST_STUB */

/** @} */

#endif /* LAYERED_CAR_BSP_I2C_H */
