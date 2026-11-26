/**
 * @file led.h
 * @brief 轻量级 GPIO / PWM LED 驱动（合并开关与 16 位亮度调光）。
 * @addtogroup drivers
 * @{
 */

#ifndef LAYERED_CAR_LED_H
#define LAYERED_CAR_LED_H

#include "bsp_def.h"
#include "bsp_gpio.h"
#include "bsp_pwm.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief LED 硬件驱动模式。
 */
typedef enum dev_led_mode_e { DEV_LED_MODE_GPIO = 0U, DEV_LED_MODE_PWM } dev_led_mode_t;

/**
 * @brief LED 初始化配置（GPIO 模式用 gpio/active_high，PWM 模式用 pwm）。
 */
typedef struct dev_led_cfg_s {
  dev_led_mode_t mode;
  bsp_gpio_t gpio;
  bool active_high;
  bsp_pwm_t pwm;
} dev_led_cfg_t;

/**
 * @brief LED 运行期静态对象。
 */
typedef struct dev_led_s {
  dev_led_mode_t mode;
  bsp_gpio_t gpio;
  bool active_high;
  bsp_pwm_t pwm;
  bool initialized;
  bool is_on;
  uint16_t brightness;
} dev_led_t;

/** @brief 初始化 GPIO 或 PWM LED 并默认置为熄灭状态。 */
bsp_status_t Dev_LED_Init(dev_led_t *led, const dev_led_cfg_t *cfg);

/** @brief 设置 LED 开关状态（PWM 模式下开为满亮度、关为 0）。 */
void Dev_LED_Set(dev_led_t *led, bool on);

/** @brief 翻转 LED 当前开关状态。 */
void Dev_LED_Toggle(dev_led_t *led);

/** @brief 设置 16 位亮度 0..UINT16_MAX（GPIO 模式下非 0 即亮）。 */
void Dev_LED_SetBrightness(dev_led_t *led, uint16_t brightness);

/** @brief 查询当前逻辑点亮状态。 */
bool Dev_LED_IsOn(const dev_led_t *led);

/** @} */

#endif /* LAYERED_CAR_LED_H */
