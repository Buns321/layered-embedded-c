//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file led_pwm.c
 * @brief PWM LED 驱动实现。
 * @ingroup drivers_concrete
 */

#include "led_pwm.h"
#include <assert.h>

#define LED_PWM_BRIGHTNESS_MAX UINT16_MAX

/**
 * @brief 向 PWM 通道写入亮度。
 */
static void dev_led_pwm_writebrightness(led_pwm_t *led_pwm_h, uint16_t brightness) {
  assert(led_pwm_h);
  (void)Platform_PWM_SetDuty(
    led_pwm_h->pwm_instance,
    led_pwm_h->pwm_channel,
    LED_PWM_BRIGHTNESS_MAX,
    brightness
  );
}

/**
 * @brief 以当前亮度打开 PWM LED。
 */
static void dev_led_pwm_turnon(led_base_t *led_h) {
  assert(led_h);
  dev_led_pwm_writebrightness((led_pwm_t *)led_h, LED_PWM_BRIGHTNESS_MAX);
}

/**
 * @brief 将 PWM 占空比置零关闭 LED。
 */
static void dev_led_pwm_turnoff(led_base_t *led_h) {
  assert(led_h);
  dev_led_pwm_writebrightness((led_pwm_t *)led_h, 0u);
}

/**
 * @brief 更新 PWM LED 亮度。
 */
static void dev_led_pwm_setbrightness(led_base_t *led_h, uint16_t brightness) {
  assert(led_h);
  dev_led_pwm_writebrightness((led_pwm_t *)led_h, brightness);
}

static const led_ops_t led_ops_pwm = {
  .on             = dev_led_pwm_turnon,
  .off            = dev_led_pwm_turnoff,
  .set_brightness = dev_led_pwm_setbrightness,
};

/**
 * @brief 初始化 PWM LED。
 * @param led_h 对象。
 * @param name 名称。
 * @param pwm_instance 定时器身份。
 * @param pwm_channel 通道身份。
 * @retval PLATFORM_STATUS_OK PWM 启动成功。
 * @retval PLATFORM_STATUS_ERROR PWM 启动失败。
 * @note 初始化时先把占空比置 0。
 */
platform_status_t Dev_LED_PWM_Init(
  led_pwm_t             *led_h,
  const char            *name,
  platform_pwm_instance_t  pwm_instance,
  platform_pwm_channel_t   pwm_channel
) {
  assert(led_h && name);

  Dev_LED_Base_Init(&led_h->led_base, name, &led_ops_pwm);
  led_h->pwm_instance = pwm_instance;
  led_h->pwm_channel  = pwm_channel;

  dev_led_pwm_writebrightness(led_h, 0u);

  return Platform_PWM_Start(pwm_instance, pwm_channel);
}
