//
// Created by Buns_ on 2026/8/15.
//

#include "../Inc/led_gpio.h"
#include <assert.h>

static void Dev_LED_GPIO_ON(led_base_t *this) {
  assert(this);
  led_gpio_t *self = (led_gpio_t *)this;
  switch (self->active_level) {
    case PLATFORM_GPIO_PIN_SET:
      Platform_GPIO_WritePin(self->port, self->pin, PLATFORM_GPIO_PIN_SET);
      break;
    case PLATFORM_GPIO_PIN_RESET:
      Platform_GPIO_WritePin(self->port, self->pin, PLATFORM_GPIO_PIN_RESET);
      break;
    default: assert(0); break;
  }
}

static void Dev_LED_GPIO_OFF(led_base_t *this) {
  assert(this);
  led_gpio_t *self = (led_gpio_t *)this;
  switch (self->active_level) {
    case PLATFORM_GPIO_PIN_SET:
      Platform_GPIO_WritePin(self->port, self->pin, PLATFORM_GPIO_PIN_RESET);
      break;
    case PLATFORM_GPIO_PIN_RESET:
      Platform_GPIO_WritePin(self->port, self->pin, PLATFORM_GPIO_PIN_SET);
      break;
    default: assert(0); break;
  }
}

static void Dev_LED_GPIO_SetBrightness(led_base_t *this, uint16_t brightness) {
  // 空函数用于调用参数，防止编译(编译前期的语法/语义分析阶段)产生 unused parameter 警告
  // 由于优化器作用与编译后期，因此空语句被优化不影响防止产生 unused parameter 警告的作用
  // 不可使用 assert(this && brightness) 语句以求防止产生 unused parameter 警告，这是因为在发布阶段定义 NODEBUG(Release) 后，assert() 会被展开为 (void)0
  (void)this; (void)brightness; assert(0);
}

static const led_ops_t led_ops_gpio = {
  .on             = Dev_LED_GPIO_ON,
  .off            = Dev_LED_GPIO_OFF,
  .set_brightness = Dev_LED_GPIO_SetBrightness,
};

void Dev_LED_GPIO_Init(led_gpio_t *this, const char *name, const Platform_GPIO_Port port, const uint16_t pin, const Platform_GPIO_Pin_State active_level) {
  assert(this && name);
  Dev_LED_Base_Init(&this->led_base, name, &led_ops_gpio);
  this->port = port;
  this->pin = pin;
  this->active_level = active_level;
}
