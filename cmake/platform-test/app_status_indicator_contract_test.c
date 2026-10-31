/**
 * @file app_status_indicator_contract_test.c
 * @brief 快速通道任务一蜂鸣器和 LED 提示契约测试。
 * @ingroup tests
 */

#include "app_status_indicator.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

static uint32_t s_init_calls;

/**
 * @brief LED GPIO 初始化测试替身。
 * @param led_h 非空 LED 对象。
 * @param name 非空名称。
 * @param port GPIO 端口；测试忽略。
 * @param pin 引脚掩码；测试忽略。
 * @param active_level 有效电平；测试保存。
 * @pre 参数符合真实驱动契约。
 * @post base 置为关闭并记录初始化次数。
 * @note 不访问 GPIO。
 */
void Dev_LED_GPIO_Init(
  led_gpio_t *led_h,
  const char *name,
  platform_gpio_port_t port,
  uint16_t pin,
  platform_gpio_pin_state_t active_level
) {
  assert(led_h && name);
  led_h->led_base.name = name;
  led_h->port = port;
  led_h->pin = pin;
  led_h->active_level = active_level;
  led_h->led_base.is_on = DEV_LED_OFF;
  s_init_calls++;
}

/**
 * @brief LED 开启测试替身。
 * @param led_h 非空 LED 基类。
 * @pre led_h 已初始化。
 * @post is_on 置为 DEV_LED_ON。
 * @note 不访问 GPIO。
 */
void Dev_LED_Base_TurnOn(led_base_t *led_h) {
  assert(led_h);
  led_h->is_on = DEV_LED_ON;
}

/**
 * @brief LED 关闭测试替身。
 * @param led_h 非空 LED 基类。
 * @pre led_h 已初始化。
 * @post is_on 置为 DEV_LED_OFF。
 * @note 不访问 GPIO。
 */
void Dev_LED_Base_TurnOff(led_base_t *led_h) {
  assert(led_h);
  led_h->is_on = DEV_LED_OFF;
}

/**
 * @brief 验证初始化、短鸣、长鸣和 LED 翻转。
 * @post 两个输出初始化，短鸣和长鸣时序正确，LED 在 250 ms 翻转。
 * @note 使用 LED 驱动替身，不访问硬件。
 */
static void test_indicator(void) {
  app_status_indicator_t indicator;
  App_StatusIndicator_Init(&indicator);
  assert(s_init_calls == 2u);
  assert(indicator.buzzer_h.led_base.is_on == DEV_LED_OFF);
  assert(indicator.led_h.led_base.is_on == DEV_LED_OFF);

  App_StatusIndicator_Notify(
    &indicator,
    APP_CALIBRATION_EVENT_FACE_SAVED,
    100u
  );
  assert(indicator.buzzer_h.led_base.is_on == DEV_LED_ON);
  App_StatusIndicator_Process(
    &indicator,
    APP_FAST_CHANNEL_STATE_WAIT_FACE,
    180u
  );
  assert(indicator.buzzer_h.led_base.is_on == DEV_LED_OFF);
  App_StatusIndicator_Process(
    &indicator,
    APP_FAST_CHANNEL_STATE_WAIT_FACE,
    300u
  );
  assert(indicator.buzzer_h.led_base.is_on == DEV_LED_ON);
  App_StatusIndicator_Process(
    &indicator,
    APP_FAST_CHANNEL_STATE_WAIT_FACE,
    380u
  );
  assert(indicator.buzzer_h.led_base.is_on == DEV_LED_OFF);

  App_StatusIndicator_Notify(
    &indicator,
    APP_CALIBRATION_EVENT_MAIN_ENTERED,
    1000u
  );
  assert(indicator.buzzer_h.led_base.is_on == DEV_LED_ON);
  App_StatusIndicator_Process(
    &indicator,
    APP_FAST_CHANNEL_STATE_MAIN,
    1600u
  );
  assert(indicator.buzzer_h.led_base.is_on == DEV_LED_OFF);
}

/**
 * @brief 运行状态指示器契约测试。
 * @return 全部断言通过后返回 0。
 * @post 测试不保留外部资源。
 * @note 任一断言失败会终止进程。
 */
int main(void) {
  test_indicator();
  return 0;
}
