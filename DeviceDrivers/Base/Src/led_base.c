//
// 由 Buns_ 创建于 2026/8/15。
//

/**
 * @file led_base.c
 * @brief LED 抽象接口实现。
 * @ingroup drivers_base
 */

#include "led_base.h"
#include <assert.h>

void Dev_LED_Base_Init(led_base_t *led_h, const char *name, const led_ops_t *ops) {
  assert(led_h && name && ops);
  led_h->ops        = ops;
  led_h->name       = name;
  led_h->is_on      = DEV_LED_OFF;
}

dev_led_state_t Dev_LED_Base_ReadStatus(const led_base_t *led_h) {
  assert(led_h);
  return led_h->is_on;
}

void Dev_LED_Base_SetBrightness(led_base_t *led_h, uint16_t brightness) {
  assert(led_h && led_h->ops && led_h->ops->set_brightness);
  led_h->ops->set_brightness(led_h, brightness);
  led_h->is_on = brightness == 0 ? DEV_LED_OFF : DEV_LED_ON;
}

void Dev_LED_Base_TurnOn(led_base_t *led_h) {
  assert(led_h && led_h->ops && led_h->ops->on);
  led_h->ops->on(led_h);
  led_h->is_on = DEV_LED_ON;
}

void Dev_LED_Base_TurnOff(led_base_t *led_h) {
  assert(led_h && led_h->ops && led_h->ops->off);
  led_h->ops->off(led_h);
  led_h->is_on = DEV_LED_OFF;
}

void Dev_LED_Base_Toggle(led_base_t *led_h) {
  assert(led_h);
  switch (led_h->is_on) {
    case DEV_LED_ON:  Dev_LED_Base_TurnOff(led_h); break;
    case DEV_LED_OFF: Dev_LED_Base_TurnOn(led_h);  break;
    default:          assert(0);               break;
  }
}
