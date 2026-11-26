/**
 * @file led.c
 * @brief 轻量级 GPIO / PWM LED 驱动实现。
 * @details 属于 Framework/Drivers 设备驱动层，负责参数校验、16 位亮度到 PWM
 *          占空比的 64 位防溢出换算，以及 GPIO 有效电平极性映射。
 * @ingroup Framework_Drivers
 */

#include "led.h"
#include <stddef.h>

/**
 * @brief 校验 PWM 句柄及其定时器通道枚举值是否合法。
 * @param[in] pwm 待校验的 PWM 句柄指针。
 * @return 句柄是否合法。
 * @retval true  htim 非空且 channel 为 TIM_CHANNEL_1..4 之一。
 * @retval false 句柄为空或通道枚举非法。
 */
static bool led_pwm_isvalid(const bsp_pwm_t *pwm) {
  if (pwm == NULL || pwm->htim == NULL) {
    return false;
  }
  return pwm->channel == TIM_CHANNEL_1 || pwm->channel == TIM_CHANNEL_2 ||
         pwm->channel == TIM_CHANNEL_3 || pwm->channel == TIM_CHANNEL_4;
}

/**
 * @brief 将 16 位亮度值应用到底层 GPIO 或 PWM 硬件输出。
 * @param[in,out] led        已初始化的 LED 对象指针。
 * @param[in]     brightness 目标 16 位亮度（0 熄灭，非 0 点亮）。
 */
static void led_apply_output(dev_led_t *led, uint16_t brightness) {
  led->brightness = brightness;
  led->is_on = (brightness > 0U);
  if (led->mode == DEV_LED_MODE_GPIO) {
    // 根据有效电平极性将逻辑开关量映射为实际引脚电平
    bool level = led->is_on ? led->active_high : !led->active_high;
    BSP_GPIO_Write(led->gpio, level);
  } else {
    // 先升至 uint64_t 再做乘法，防止 max_duty * UINT16_MAX 在 32 位运算下溢出
    uint32_t duty = (uint32_t)(((uint64_t)led->pwm.max_duty * (uint64_t)brightness) / UINT16_MAX);
    (void)BSP_PWM_SetDuty(&led->pwm, duty);
  }
}

bsp_status_t Dev_LED_Init(dev_led_t *led, const dev_led_cfg_t *cfg) {
  if (led == NULL || cfg == NULL) {
    return BSP_ERR_INVALID_ARG;
  }

  if (cfg->mode == DEV_LED_MODE_GPIO) {
    if (!BSP_GPIO_IsValid(cfg->gpio)) {
      return BSP_ERR_INVALID_ARG;
    }
    led->mode = DEV_LED_MODE_GPIO;
    led->gpio = cfg->gpio;
    led->active_high = cfg->active_high;
    led->pwm = (bsp_pwm_t){0};
    led->initialized = true;
    led_apply_output(led, 0U);
    return BSP_OK;
  }

  if (cfg->mode == DEV_LED_MODE_PWM) {
    if (!led_pwm_isvalid(&cfg->pwm)) {
      return BSP_ERR_INVALID_ARG;
    }
    led->mode = DEV_LED_MODE_PWM;
    led->gpio = (bsp_gpio_t){0};
    led->active_high = false;
    led->pwm = cfg->pwm;
    // 未显式指定 max_duty 时默认对齐 16 位满量程 UINT16_MAX
    if (led->pwm.max_duty == 0U) {
      led->pwm.max_duty = UINT16_MAX;
    }
    led->initialized = true;
    led_apply_output(led, 0U);
    return BSP_PWM_Start(&led->pwm);
  }

  return BSP_ERR_INVALID_ARG;
}

void Dev_LED_Set(dev_led_t *led, bool on) {
  if (led == NULL || !led->initialized) {
    return;
  }
  led_apply_output(led, on ? UINT16_MAX : 0U);
}

void Dev_LED_Toggle(dev_led_t *led) {
  if (led == NULL || !led->initialized) {
    return;
  }
  Dev_LED_Set(led, !led->is_on);
}

void Dev_LED_SetBrightness(dev_led_t *led, uint16_t brightness) {
  if (led == NULL || !led->initialized) {
    return;
  }
  led_apply_output(led, brightness);
}

bool Dev_LED_IsOn(const dev_led_t *led) { return (led != NULL) && led->initialized && led->is_on; }
