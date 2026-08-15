//
// Created by Buns_ on 2026/8/15.
//

#include "../Inc/led_base.h"

#include <assert.h>

void Dev_LED_Base_Init(led_base_t *this, const char *name, const led_ops_t *ops) {
  assert(this && name && ops);
  this->ops        = ops;
  this->name       = name;
  this->is_on      = DEV_LED_OFF;
}

Dev_LED_State Dev_LED_Base_ReadStatus(const led_base_t *this) {
  assert(this);
  return this->is_on;
}

void Dev_LED_Base_SetBrightness(led_base_t *this, const uint16_t brightness) {
  assert(this && this->ops && this->ops->set_brightness);
  this->ops->set_brightness(this, brightness);
}

void Dev_LED_Base_ON(led_base_t *this) {
  assert(this && this->ops && this->ops->on);
  this->ops->on(this);
  this->is_on = DEV_LED_ON;
}

void Dev_LED_Base_OFF(led_base_t *this) {
  assert(this && this->ops && this->ops->off);
  this->ops->off(this);
  this->is_on = DEV_LED_OFF;
}

void Dev_LED_Base_Toggle(led_base_t *this) {
  assert(this);
  switch (this->is_on) {
    case DEV_LED_ON:  Dev_LED_Base_OFF(this); break;
    case DEV_LED_OFF: Dev_LED_Base_ON(this);  break;
    default:          assert(0);               break;
  }
}
