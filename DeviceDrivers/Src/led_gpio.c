//
// 由 Buns_ 创建于 2026/8/15。
//

/**
 * @file led_gpio.c
 * @brief GPIO LED 驱动实现。
 * @ingroup drivers_concrete
 */

#include "led_gpio.h"
#include <assert.h>

/**
 * @brief 按有效电平打开 GPIO LED。
 */
static void dev_led_gpio_turnon(led_base_t *led_h) {
  assert(led_h);
  led_gpio_t *led_gpio_h = (led_gpio_t *)led_h;
  switch (led_gpio_h->active_level) {
    case PLATFORM_GPIO_PIN_SET:
      Platform_GPIO_WritePin(led_gpio_h->port, led_gpio_h->pin, PLATFORM_GPIO_PIN_SET);
      break;
    case PLATFORM_GPIO_PIN_RESET:
      Platform_GPIO_WritePin(led_gpio_h->port, led_gpio_h->pin, PLATFORM_GPIO_PIN_RESET);
      break;
    default: assert(0); break;
  }
}

/**
 * @brief 按有效电平关闭 GPIO LED。
 */
static void dev_led_gpio_turnoff(led_base_t *led_h) {
  assert(led_h);
  led_gpio_t *led_gpio_h = (led_gpio_t *)led_h;
  switch (led_gpio_h->active_level) {
    case PLATFORM_GPIO_PIN_SET:
      Platform_GPIO_WritePin(led_gpio_h->port, led_gpio_h->pin, PLATFORM_GPIO_PIN_RESET);
      break;
    case PLATFORM_GPIO_PIN_RESET:
      Platform_GPIO_WritePin(led_gpio_h->port, led_gpio_h->pin, PLATFORM_GPIO_PIN_SET);
      break;
    default: assert(0); break;
  }
}

/**
 * @brief 将非零亮度映射为开灯，零亮度映射为关灯。
 */
static void dev_led_gpio_setbrightness(led_base_t *led_h, uint16_t brightness) {
  // 空函数用于调用参数，防止编译(编译前期的语法/语义分析阶段)产生 unused parameter 警告
  // 由于优化器作用与编译后期，因此空语句被优化不影响防止产生 unused parameter 警告的作用
  // 不可使用 assert(led_h && brightness) 语句以求防止产生 unused parameter 警告，这是因为在发布阶段定义 NODEBUG(Release) 后，assert() 会被展开为 (void)0
  (void)led_h; (void)brightness; assert(0);
}

static const led_ops_t led_ops_gpio = {
  .on             = dev_led_gpio_turnon,
  .off            = dev_led_gpio_turnoff,
  .set_brightness = dev_led_gpio_setbrightness,
};

/**
 * @brief 初始化 GPIO LED。
 * @param led_h 对象。
 * @param name 名称。
 * @param port 端口。
 * @param pin 引脚掩码。
 * @param active_level 点亮电平。
 * @note 初始化会显式写入 inactive level。
 */
void Dev_LED_GPIO_Init(led_gpio_t *led_h, const char *name, platform_gpio_port_t port, uint16_t pin, platform_gpio_pin_state_t active_level) {
  assert(led_h && name);
  Dev_LED_Base_Init(&led_h->led_base, name, &led_ops_gpio);
  led_h->port = port;
  led_h->pin = pin;
  led_h->active_level = active_level;

  platform_gpio_pin_state_t inactive_level =
    (active_level == PLATFORM_GPIO_PIN_SET) ? PLATFORM_GPIO_PIN_RESET : PLATFORM_GPIO_PIN_SET;
  Platform_GPIO_WritePin(port, pin, inactive_level);
}
