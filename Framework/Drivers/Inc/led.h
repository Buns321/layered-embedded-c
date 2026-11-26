/**
 * @file led.h
 * @brief 轻量级 GPIO / PWM LED 驱动接口（合并开关与 16 位亮度调光）。
 * @details 属于 Framework/Drivers 设备驱动层，基于 BSP_GPIO 与 BSP_PWM 实现
 *          纯静态分配的 LED 控制，支持低电平/高电平有效 GPIO 驱动及定时器 PWM 调光。
 *
 * @addtogroup Framework_Drivers
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
typedef enum dev_led_mode_e {
  DEV_LED_MODE_GPIO = 0U, /**< GPIO 开关量驱动模式 */
  DEV_LED_MODE_PWM        /**< 定时器 PWM 亮度调光模式 */
} dev_led_mode_t;

/**
 * @brief LED 初始化配置结构体。
 * @details GPIO 模式下仅使用 gpio 与 active_high；PWM 模式下仅使用 pwm。
 */
typedef struct dev_led_cfg_s {
  dev_led_mode_t mode; /**< 驱动模式（GPIO 或 PWM） */
  bsp_gpio_t gpio;     /**< GPIO 引脚句柄（DEV_LED_MODE_GPIO 时必须有效） */
  bool active_high;    /**< 有效电平极性（true 为高电平点亮，false 为低电平点亮） */
  bsp_pwm_t pwm; /**< PWM 通道句柄（DEV_LED_MODE_PWM 时必须有效，max_duty=0 默认 UINT16_MAX） */
} dev_led_cfg_t;

/**
 * @brief LED 运行期静态对象结构体。
 */
typedef struct dev_led_s {
  dev_led_mode_t mode; /**< 当前驱动模式 */
  bsp_gpio_t gpio;     /**< GPIO 引脚句柄副本 */
  bool active_high;    /**< 有效电平极性副本 */
  bsp_pwm_t pwm;       /**< PWM 通道句柄副本（含解析后的 max_duty） */
  bool initialized;    /**< 初始化完成标志 */
  bool is_on;          /**< 当前逻辑点亮状态（brightness > 0 时为 true） */
  uint16_t brightness; /**< 当前 16 位亮度值（范围 [0, UINT16_MAX]，无量纲） */
} dev_led_t;

/**
 * @brief 初始化 GPIO 或 PWM LED 并默认置为熄灭状态。
 * @param[out] led 待初始化的 LED 静态对象指针（非 NULL）。
 * @param[in]  cfg 初始化配置结构体指针（非 NULL）。
 * @return 初始化状态码。
 * @retval BSP_OK              初始化成功且已置为熄灭状态（PWM 模式已启动输出）。
 * @retval BSP_ERR_INVALID_ARG 参数为空、模式非法或 GPIO/PWM 句柄无效。
 * @retval 其他 bsp_status_t   底层 BSP_PWM_Start 返回的错误码。
 * @pre 对应 GPIO 时钟或 TIM 外设已完成 CubeMX 初始化。
 */
bsp_status_t Dev_LED_Init(dev_led_t *led, const dev_led_cfg_t *cfg);

/**
 * @brief 设置 LED 开关状态。
 * @details PWM 模式下点亮对应满亮度（UINT16_MAX），熄灭对应 0 占空比。
 * @param[in,out] led LED 对象指针（为 NULL 或未初始化时安全忽略）。
 * @param[in]     on  目标开关状态（true 点亮，false 熄灭）。
 * @pre 须先调用 Dev_LED_Init() 成功初始化。
 */
void Dev_LED_Set(dev_led_t *led, bool on);

/**
 * @brief 翻转 LED 当前逻辑开关状态。
 * @param[in,out] led LED 对象指针（为 NULL 或未初始化时安全忽略）。
 * @pre 须先调用 Dev_LED_Init() 成功初始化。
 */
void Dev_LED_Toggle(dev_led_t *led);

/**
 * @brief 设置 16 位亮度值。
 * @details PWM 模式下按比例映射到 [0, max_duty]；GPIO 模式下非 0 即点亮、0 即熄灭。
 * @param[in,out] led        LED 对象指针（为 NULL 或未初始化时安全忽略）。
 * @param[in]     brightness 目标亮度值（范围 [0, UINT16_MAX]）。
 * @pre 须先调用 Dev_LED_Init() 成功初始化。
 */
void Dev_LED_SetBrightness(dev_led_t *led, uint16_t brightness);

/**
 * @brief 查询 LED 当前逻辑点亮状态。
 * @param[in] led LED 对象指针（允许为 NULL）。
 * @return 逻辑点亮状态。
 * @retval true  已初始化且当前处于点亮状态（brightness > 0）。
 * @retval false 指针为空、未初始化或当前处于熄灭状态。
 */
bool Dev_LED_IsOn(const dev_led_t *led);

#endif /* LAYERED_CAR_LED_H */

/** @} */
