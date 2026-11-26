/**
 * @file bsp_gpio.h
 * @brief 轻量级 GPIO 引脚描述符与读写翻转接口。
 * @details 以值传递的 `bsp_gpio_t` 描述符封装端口寄存器指针与单比特引脚掩码，
 *          支持可选未连接引脚（port == NULL）的空操作安全保护，零静态状态、零动态分配。
 *
 * @addtogroup BSP
 * @{
 */

#ifndef LAYERED_CAR_BSP_GPIO_H
#define LAYERED_CAR_BSP_GPIO_H

#include "bsp_mcu.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief GPIO 引脚描述符。
 * @details 当 `port == NULL` 且 `pin == 0` 时表示可选引脚未连接，读写操作将安全忽略。
 */
typedef struct bsp_gpio_s {
  GPIO_TypeDef *port; /**< GPIO 端口基址指针（如 GPIOA..GPIOE）；NULL 表示未连接。 */
  uint16_t pin;       /**< 引脚位掩码（如 GPIO_PIN_0..GPIO_PIN_15，须为单比特非零值）。 */
} bsp_gpio_t;

/**
 * @brief 校验 GPIO 描述符是否有效。
 * @param[in] gpio 待校验的 GPIO 描述符。
 * @return 描述符是否有效。
 * @retval true  `port != NULL` 且 `pin` 为非零单比特掩码。
 * @retval false `port == NULL`、`pin == 0` 或 `pin` 包含多个置位比特。
 * @note 纯计算检查，不访问硬件寄存器，可在任意上下文调用。
 */
bool BSP_GPIO_IsValid(bsp_gpio_t gpio);

/**
 * @brief 写入 GPIO 输出电平。
 * @param[in] gpio 目标 GPIO 描述符；无效描述符会被安全忽略。
 * @param[in] high true 输出高电平（GPIO_PIN_SET），false 输出低电平（GPIO_PIN_RESET）。
 * @note 底层通过 BSRR 原子置位/复位寄存器实现，可在任务或 ISR 上下文调用。
 */
void BSP_GPIO_Write(bsp_gpio_t gpio, bool high);

/**
 * @brief 读取 GPIO 输入电平。
 * @param[in] gpio 目标 GPIO 描述符。
 * @return 引脚当前逻辑电平。
 * @retval true  引脚处于高电平（GPIO_PIN_SET）。
 * @retval false 引脚处于低电平或描述符无效。
 * @note 可在任务或 ISR 上下文安全调用。
 */
bool BSP_GPIO_Read(bsp_gpio_t gpio);

/**
 * @brief 翻转 GPIO 输出电平。
 * @param[in] gpio 目标 GPIO 描述符；无效描述符会被安全忽略。
 * @note 内部调用 HAL_GPIO_TogglePin()，可在任务或 ISR 上下文调用。
 */
void BSP_GPIO_Toggle(bsp_gpio_t gpio);

#endif /* LAYERED_CAR_BSP_GPIO_H */

/** @} */
