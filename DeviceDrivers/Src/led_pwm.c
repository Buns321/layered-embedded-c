//
// 由 Buns_ 创建于 2026/10/20。
//

#include "led_pwm.h"
#include <assert.h>

#define LED_PWM_BRIGHTNESS_MAX UINT16_MAX

static void Dev_LED_PWM_WriteBrightness(led_pwm_t *led_pwm_h, uint16_t brightness) {
  assert(led_pwm_h);
  (void)Platform_PWM_SetDuty(
    led_pwm_h->pwm_instance,
    led_pwm_h->pwm_channel,
    LED_PWM_BRIGHTNESS_MAX,
    brightness
  );
}

static void Dev_LED_PWM_TurnOn(led_base_t *led_h) {
  assert(led_h);
  Dev_LED_PWM_WriteBrightness((led_pwm_t *)led_h, LED_PWM_BRIGHTNESS_MAX);
}

static void Dev_LED_PWM_TurnOff(led_base_t *led_h) {
  assert(led_h);
  Dev_LED_PWM_WriteBrightness((led_pwm_t *)led_h, 0u);
}

static void Dev_LED_PWM_SetBrightness(led_base_t *led_h, uint16_t brightness) {
  assert(led_h);
  Dev_LED_PWM_WriteBrightness((led_pwm_t *)led_h, brightness);
}

static const led_ops_t led_ops_pwm = {
  .on             = Dev_LED_PWM_TurnOn,
  .off            = Dev_LED_PWM_TurnOff,
  .set_brightness = Dev_LED_PWM_SetBrightness,
};

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

  Dev_LED_PWM_WriteBrightness(led_h, 0u);

  return Platform_PWM_Start(pwm_instance, pwm_channel);
}
