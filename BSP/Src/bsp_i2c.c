/**
 * @file bsp_i2c.c
 * @brief 轻量主机 I2C 探测与寄存器读写实现。
 * @details 统一校验 7 位从机地址与寄存器宽度后，将地址左移 1 位以适配 STM32 HAL
 *          8 位地址约定，并转发至阻塞式 HAL_I2C_IsDeviceReady/Mem_Write/Mem_Read。
 * @ingroup BSP
 */

#include "bsp_i2c.h"

#if defined(HAL_I2C_MODULE_ENABLED) || defined(BSP_HOST_STUB)

#include "bsp_hal_internal.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief 校验 I2C 从设备描述符字段是否合法。
 * @param[in] dev I2C 从设备描述符指针。
 * @return 是否合法（非空、句柄有效、7 位地址 <= 0x7F 且超时 > 0 ms）。
 */
static bool bsp_i2c_isvaliddev(const bsp_i2c_t *dev) {
  return dev != NULL && dev->hi2c != NULL && dev->addr_7bit <= 0x7FU && dev->timeout_ms > 0U;
}

/**
 * @brief 将 7 位 I2C 从机地址左移 1 位转换为 HAL 要求的 8 位地址格式。
 * @param[in] dev I2C 从设备描述符指针。
 * @return 左移后的 8 位设备地址。
 */
static uint16_t bsp_i2c_haladdr(const bsp_i2c_t *dev) {
  // STM32 HAL I2C API 要求传入左移 1 位后的从机地址（Bit0 由硬件自动填充 R/W 位）
  return (uint16_t)((uint16_t)dev->addr_7bit << 1U);
}

/**
 * @brief 将字节表示的寄存器地址宽度映射为 HAL 宏定义。
 * @param[in]  reg_size     寄存器地址宽度（1 或 2 byte）。
 * @param[out] out_hal_size 输出对应的 HAL 宏（`I2C_MEMADD_SIZE_8BIT` 或 `16BIT`）。
 * @return 映射是否成功。
 */
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
  // HAL_I2C_Mem_Write 原型缺少 const 修饰，此处强转仅用于适配 HAL 签名，底层只读不写
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

/** @brief 未启用 HAL_I2C_MODULE_ENABLED 且非主机桩时的 ISO C11 非空翻译单元占位类型。 */
typedef int bsp_i2c_empty_tu_t;

#endif /* HAL_I2C_MODULE_ENABLED || BSP_HOST_STUB */
