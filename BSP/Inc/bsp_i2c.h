/**
 * @file bsp_i2c.h
 * @brief 轻量主机 I2C 设备探测与寄存器阻塞读写接口。
 * @details 封装 STM32 HAL I2C 主机阻塞操作（按 `HAL_I2C_MODULE_ENABLED` 或 `BSP_HOST_STUB`
 *          条件编译），统一接收 7 位从机地址并在内部自动左移 1 位适配 HAL 协议。
 *          本模块无内部状态与动态分配。
 *
 * @addtogroup BSP
 * @{
 */

#ifndef LAYERED_CAR_BSP_I2C_H
#define LAYERED_CAR_BSP_I2C_H

#include "bsp_def.h"
#include "bsp_mcu.h"
#include <stdint.h>

#if defined(HAL_I2C_MODULE_ENABLED) || defined(BSP_HOST_STUB)

/**
 * @brief I2C 从设备描述符。
 * @details `addr_7bit` 使用标准 7 位地址（0x00..0x7F），调用 HAL 接口时内部自动左移 1 位。
 */
typedef struct bsp_i2c_s {
  I2C_HandleTypeDef *hi2c; /**< I2C 外设 HAL 句柄指针（非 NULL，须已完成初始化）。 */
  uint8_t addr_7bit;       /**< 7 位 I2C 从机地址（范围 0x00..0x7F，不含读写位）。 */
  uint32_t timeout_ms;     /**< HAL 阻塞传输超时时间（单位：ms，必须 > 0）。 */
} bsp_i2c_t;

/**
 * @brief 探测指定 I2C 从设备是否在总线上应答 ACK。
 * @param[in] dev    I2C 从设备描述符指针（不允许为 NULL）。
 * @param[in] trials 最大探测重试次数（须 > 0）。
 * @return 操作状态码。
 * @retval BSP_OK              从设备正常应答。
 * @retval BSP_ERR_INVALID_ARG 描述符字段非法或 `trials == 0`。
 * @retval BSP_ERR_TIMEOUT     总线或从设备响应超时。
 * @retval BSP_ERR_BUSY        I2C 总线正忙。
 * @retval BSP_ERR_HAL         未应答或其他 HAL 总线错误。
 * @note 仅限任务上下文阻塞调用，禁止在 ISR 中使用。
 */
bsp_status_t BSP_I2C_Probe(const bsp_i2c_t *dev, uint32_t trials);

/**
 * @brief 阻塞方式向 I2C 从设备指定寄存器写入数据。
 * @param[in] dev      I2C 从设备描述符指针（不允许为 NULL）。
 * @param[in] reg      目标寄存器地址。
 * @param[in] reg_size 寄存器地址宽度（单位：byte，仅支持 1 或 2）。
 * @param[in] data     待写入数据缓冲区指针（不允许为 NULL）。
 * @param[in] len      写入数据长度（单位：byte，须 > 0）。
 * @return 操作状态码。
 * @retval BSP_OK              写入成功。
 * @retval BSP_ERR_INVALID_ARG 描述符非法、`data == NULL`、`len == 0` 或 `reg_size` 非 1/2。
 * @retval BSP_ERR_TIMEOUT     传输超时。
 * @retval BSP_ERR_BUSY        总线正忙。
 * @retval BSP_ERR_HAL         HAL 传输错误。
 * @note 仅限任务上下文阻塞调用。
 */
bsp_status_t BSP_I2C_MemWrite(const bsp_i2c_t *dev, uint16_t reg, uint16_t reg_size,
                              const uint8_t *data, uint16_t len);

/**
 * @brief 阻塞方式从 I2C 从设备指定寄存器读取数据。
 * @param[in]  dev      I2C 从设备描述符指针（不允许为 NULL）。
 * @param[in]  reg      目标寄存器地址。
 * @param[in]  reg_size 寄存器地址宽度（单位：byte，仅支持 1 或 2）。
 * @param[out] data     接收数据缓冲区指针（不允许为 NULL）。
 * @param[in]  len      读取数据长度（单位：byte，须 > 0）。
 * @return 操作状态码。
 * @retval BSP_OK              读取成功。
 * @retval BSP_ERR_INVALID_ARG 描述符非法、`data == NULL`、`len == 0` 或 `reg_size` 非 1/2。
 * @retval BSP_ERR_TIMEOUT     传输超时。
 * @retval BSP_ERR_BUSY        总线正忙。
 * @retval BSP_ERR_HAL         HAL 传输错误。
 * @note 仅限任务上下文阻塞调用。
 */
bsp_status_t BSP_I2C_MemRead(const bsp_i2c_t *dev, uint16_t reg, uint16_t reg_size, uint8_t *data,
                             uint16_t len);

#endif /* HAL_I2C_MODULE_ENABLED || BSP_HOST_STUB */

#endif /* LAYERED_CAR_BSP_I2C_H */

/** @} */
