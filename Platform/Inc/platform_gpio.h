//
// 由 Buns_ 创建于 2026/8/15。
//

/**
 * @file platform_gpio.h
 * @brief GPIO 抽象：端口/引脚枚举与读写接口。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_GPIO_H
#define LAYERED_EMBEDDED_C_PLATFORM_GPIO_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

/**
 * @brief 逻辑端口身份。
 */
typedef enum platform_gpio_port_e {
  PLATFORM_GPIO_PORT_A = 0u,
  PLATFORM_GPIO_PORT_B,
  PLATFORM_GPIO_PORT_C,
#if PLATFORM_ENABLE_GPIO_D
  PLATFORM_GPIO_PORT_D = 3u,
#endif
#if PLATFORM_ENABLE_GPIO_E
  PLATFORM_GPIO_PORT_E = 4u,
#endif
} platform_gpio_port_t;

/**
 * @brief 逻辑引脚电平。
 * @note 这是逻辑状态，实际高低电平由具体驱动/板级决定。
 */
typedef enum platform_gpio_pin_state_e {
  PLATFORM_GPIO_PIN_SET = 0u,
  PLATFORM_GPIO_PIN_RESET,
} platform_gpio_pin_state_t;

/**
 * @brief 读取 GPIO 引脚电平并刷新该端口的错误快照。
 * @param GPIOx 已由板级配置启用的端口身份。
 * @param GPIO_Pin 非零引脚掩码；正常使用应只包含一个 bit。
 * @retval PLATFORM_GPIO_PIN_SET 读到高电平。
 * @retval PLATFORM_GPIO_PIN_RESET 读到低电平，或端口/掩码非法。
 * @note 非法端口或零掩码会记录 PLATFORM_ERROR_INVALID_ARGUMENT，并返回 RESET；
 *       调用方需要区分时应读取 Platform_GPIO_GetLastError()。
 * @note 多 bit 掩码的读取语义由 HAL 决定；本接口不分配内存且不阻塞。
 */
platform_gpio_pin_state_t Platform_GPIO_ReadPin (platform_gpio_port_t GPIOx, uint16_t GPIO_Pin);

/**
 * @brief 写入 GPIO 引脚逻辑电平。
 * @param GPIOx 已由板级配置启用的端口身份。
 * @param GPIO_Pin 非零引脚掩码；正常使用应只包含一个 bit。
 * @param PinState 目标逻辑电平；取 PLATFORM_GPIO_PIN_RESET 或 PLATFORM_GPIO_PIN_SET。
 * @post 输入有效时目标引脚被写入 PinState；端口、掩码或枚举非法时硬件不变，
 *       并记录 PLATFORM_ERROR_INVALID_ARGUMENT。
 * @note 无返回值。失败只能通过 Platform_GPIO_GetLastError() 观察。
 * @note 不分配内存、不阻塞；多 bit 掩码的写入语义由 HAL 决定。
 */
void Platform_GPIO_WritePin(platform_gpio_port_t GPIOx, uint16_t GPIO_Pin, platform_gpio_pin_state_t PinState);

/**
 * @brief 翻转 GPIO 引脚逻辑电平。
 * @param GPIOx 已由板级配置启用的端口身份。
 * @param GPIO_Pin 非零引脚掩码；正常使用应只包含一个 bit。
 * @post 输入有效时目标引脚电平翻转；端口或掩码非法时硬件不变并记录
 *       PLATFORM_ERROR_INVALID_ARGUMENT。
 * @note 不分配内存、不阻塞；并发翻转同一引脚的结果取决于调用顺序。
 */
void Platform_GPIO_TogglePin(platform_gpio_port_t GPIOx, uint16_t GPIO_Pin);

/**
 * @brief 锁定 GPIO 引脚配置，阻止后续修改。
 * @param GPIOx 已由板级配置启用的端口身份。
 * @param GPIO_Pin 非零引脚掩码；正常使用应只包含一个 bit。
 * @retval PLATFORM_STATUS_OK 引脚配置锁定成功。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 端口非法、掩码为零或已锁定位图无效。
 * @retval PLATFORM_STATUS_ERROR HAL 锁定失败。
 * @warning GPIO 锁定不可逆，重新上电前不能解锁。
 * @note 应由任务上下文调用并与其他 GPIO 配置操作串行化；不分配内存。
 */
platform_status_t Platform_GPIO_LockPin(platform_gpio_port_t GPIOx, uint16_t GPIO_Pin);

/**
 * @brief 读取最近一次 GPIO 操作错误快照。
 * @param GPIOx 端口身份；必须位于内部错误快照支持的 0..4，越界值返回
 *        PLATFORM_ERROR_INVALID_ARGUMENT。
 * @return 该端口的 PLATFORM_ERROR_* 位掩码；非法端口返回
 *         PLATFORM_ERROR_INVALID_ARGUMENT。
 * @note 只读，不清除快照；后续公共 GPIO 操作开始时会清除对应端口快照。
 * @note 不访问硬件、不分配内存；并发读写同一端口快照须由调用方串行化。
 */
uint32_t Platform_GPIO_GetLastError(platform_gpio_port_t GPIOx);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_GPIO_H
