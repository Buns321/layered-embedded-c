//
// 由 Buns_ 创建于 2026/8/15。
//

/**
 * @file led_gpio.h
 * @brief GPIO LED 具体实现。
 * @addtogroup drivers_concrete
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_LED_GPIO_H
#define LAYERED_EMBEDDED_C_LED_GPIO_H

#include "led_base.h"
#include "platform_gpio.h"

/**
 * @brief GPIO LED 对象。
 * @note active_level 表示“点亮”对应的电平。
 */
typedef struct led_gpio_s {
  led_base_t              led_base;
  platform_gpio_port_t      port;
  uint16_t                pin;
  platform_gpio_pin_state_t active_level;
} led_gpio_t;

/**
 * @brief 初始化 GPIO LED 并绑定 LED 基类操作表。
 * @param led_h 待初始化对象；非 NULL，生命周期和独占访问责任由调用方承担。
 * @param name 设备名称；只读字符串，由 led_base.name 保存指针而不复制。
 * @param port GPIO 端口身份；必须是当前构建启用的 platform_gpio_port_t。
 * @param pin 单引脚掩码；不可为 0，也不可同时包含多个有效引脚位。
 * @param active_level 点亮电平，只能为 PLATFORM_GPIO_PIN_SET 或 PLATFORM_GPIO_PIN_RESET。
 * @pre 对应 GPIO 已在 CubeMX/Core 中完成时钟、模式和速度配置，且调用发生在硬件使用之前。
 * @pre led_h 未被其他执行上下文使用；led_h 和 name 为 NULL 时由调试断言拦截。
 * @post led_base 已绑定 GPIO 操作表并记录 DEV_LED_OFF，port/pin/active_level 已保存，引脚被写为 active_level 的反相电平。
 * @note 不分配或释放内存，不复制 name；本函数不验证引脚当前是否被其他外设占用。
 * @note 底层 GPIO 写操作不返回状态；引脚或端口配置错误需通过 Platform_GPIO_GetLastError 检查。
 * @warning 非 ISR 安全；初始化、开关和调光不得与同一 led_h 或同一引脚的其他访问并发。非法 active_level 不在此处断言，但会使后续开关操作在调试构建中止。
 */
void Dev_LED_GPIO_Init(led_gpio_t *led_h, const char *name, platform_gpio_port_t port, uint16_t pin, platform_gpio_pin_state_t active_level);

/** @} */

#endif //LAYERED_EMBEDDED_C_LED_GPIO_H
