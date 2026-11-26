/**
 * @file bsp_i2c.c
 * @brief 轻量主机 I2C 探测与寄存器读写实现。
 * @ingroup bsp
 */

#include "bsp_i2c.h"

#if defined(HAL_I2C_MODULE_ENABLED) || defined(BSP_HOST_STUB)

#include "bsp_hal_internal.h"
#include <stdbool.h>
#include <stddef.h>

static bool bsp_i2c_isvaliddev(const bsp_i2c_t *dev) {
  return dev != NULL && dev->hi2c != NULL && dev->addr_7bit <= 0x7FU && dev->timeout_ms > 0U;
}

static uint16_t bsp_i2c_haladdr(const bsp_i2c_t *dev) {
  return (uint16_t)((uint16_t)dev->addr_7bit << 1U);
}

static bool bsp_i2c_mapregsize(uint16_t reg_size, uint16_t *out_hal_size) {
  if (reg_size == 1U) {
    *out_hal_size = I2C_MEMADD_SIZE_8BIT;
    return true;
  }
  if (reg_size == 2U) {
    *out_hal_size = I2C_MEMADD_SIZE_16BIT;
    return true;
  }
  return false;
}

bsp_status_t BSP_I2C_Probe(const bsp_i2c_t *dev, uint32_t trials) {
  if (!bsp_i2c_isvaliddev(dev) || trials == 0U) {
    return BSP_ERR_INVALID_ARG;
  }
  return bsp_status_from_hal(
      HAL_I2C_IsDeviceReady(dev->hi2c, bsp_i2c_haladdr(dev), trials, dev->timeout_ms));
}

bsp_status_t BSP_I2C_MemWrite(const bsp_i2c_t *dev, uint16_t reg, uint16_t reg_size,
                              const uint8_t *data, uint16_t len) {
  uint16_t hal_reg_size = 0U;
  if (!bsp_i2c_isvaliddev(dev) || data == NULL || len == 0U ||
      !bsp_i2c_mapregsize(reg_size, &hal_reg_size)) {
    return BSP_ERR_INVALID_ARG;
  }
  return bsp_status_from_hal(HAL_I2C_Mem_Write(dev->hi2c, bsp_i2c_haladdr(dev), reg, hal_reg_size,
                                               (uint8_t *)data, len, dev->timeout_ms));
}

bsp_status_t BSP_I2C_MemRead(const bsp_i2c_t *dev, uint16_t reg, uint16_t reg_size, uint8_t *data,
                             uint16_t len) {
  uint16_t hal_reg_size = 0U;
  if (!bsp_i2c_isvaliddev(dev) || data == NULL || len == 0U ||
      !bsp_i2c_mapregsize(reg_size, &hal_reg_size)) {
    return BSP_ERR_INVALID_ARG;
  }
  return bsp_status_from_hal(HAL_I2C_Mem_Read(dev->hi2c, bsp_i2c_haladdr(dev), reg, hal_reg_size,
                                              data, len, dev->timeout_ms));
}

#else

typedef int bsp_i2c_empty_tu_t;

#endif /* HAL_I2C_MODULE_ENABLED || BSP_HOST_STUB */
