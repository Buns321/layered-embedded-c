//
// 由 Buns_ 创建于 2026/10/20。
//

#include "led_pwm.h"
#include <assert.h>

#define LED_PWM_BRIGHTNESS_MAX UINT16_MAX

static void Dev_LED_PWM_WriteBrightness(led_pwm_t *self, uint16_t brightness) {
  assert(self);
  (void)Platform_PWM_SetDuty(
    self->pwm_instance,
    self->pwm_channel,
    LED_PWM_BRIGHTNESS_MAX,
    brightness
  );
}

static void Dev_LED_PWM_ON(led_base_t *this) {
  assert(this);
  Dev_LED_PWM_WriteBrightness((led_pwm_t *)this, LED_PWM_BRIGHTNESS_MAX);
}

static void Dev_LED_PWM_OFF(led_base_t *this) {
  assert(this);
  Dev_LED_PWM_WriteBrightness((led_pwm_t *)this, 0u);
}

static void Dev_LED_PWM_SetBrightness(led_base_t *this, uint16_t brightness) {
  assert(this);
  Dev_LED_PWM_WriteBrightness((led_pwm_t *)this, brightness);
}

static const led_ops_t led_ops_pwm = {
  .on             = Dev_LED_PWM_ON,
  .off            = Dev_LED_PWM_OFF,
  .set_brightness = Dev_LED_PWM_SetBrightness,
};

Platform_StatusTypeDef Dev_LED_PWM_Init(
  led_pwm_t             *this,
  const char            *name,
  Platform_PWM_Instance  pwm_instance,
  Platform_PWM_Channel   pwm_channel
) {
  assert(this && name);

  Dev_LED_Base_Init(&this->led_base, name, &led_ops_pwm);
  this->pwm_instance = pwm_instance;
  this->pwm_channel  = pwm_channel;

  Dev_LED_PWM_WriteBrightness(this, 0u);

  return Platform_PWM_Start(pwm_instance, pwm_channel);
}
