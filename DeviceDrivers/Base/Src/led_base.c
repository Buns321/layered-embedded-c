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

/** @brief 初始化 LED 抽象层并绑定操作表。 */
void Dev_LED_Base_Init(led_base_t *led_h, const char *name, const led_ops_t *ops) {
  assert(led_h && name && ops);
  led_h->ops        = ops;
  led_h->name       = name;
  led_h->is_on      = DEV_LED_OFF;
}

/** @brief 读取软件记录的开关状态。 */
dev_led_state_t Dev_LED_Base_ReadStatus(const led_base_t *led_h) {
  assert(led_h);
  return led_h->is_on;
}

/**
 * @brief 设置亮度。
 * @param led_h LED 对象。
 * @param brightness 0..UINT16_MAX；0 表示关闭。
 * @note 同时更新软件状态 is_on。
 */
void Dev_LED_Base_SetBrightness(led_base_t *led_h, uint16_t brightness) {
  assert(led_h && led_h->ops && led_h->ops->set_brightness);
  led_h->ops->set_brightness(led_h, brightness);
  led_h->is_on = brightness == 0 ? DEV_LED_OFF : DEV_LED_ON;
}

/** @brief 全亮。 */
void Dev_LED_Base_TurnOn(led_base_t *led_h) {
  assert(led_h && led_h->ops && led_h->ops->on);
  led_h->ops->on(led_h);
  led_h->is_on = DEV_LED_ON;
}

/** @brief 关闭。 */
void Dev_LED_Base_TurnOff(led_base_t *led_h) {
  assert(led_h && led_h->ops && led_h->ops->off);
  led_h->ops->off(led_h);
  led_h->is_on = DEV_LED_OFF;
}

/** @brief 按当前软件状态翻转。 */
void Dev_LED_Base_Toggle(led_base_t *led_h) {
  assert(led_h);
  switch (led_h->is_on) {
    case DEV_LED_ON:  Dev_LED_Base_TurnOff(led_h); break;
    case DEV_LED_OFF: Dev_LED_Base_TurnOn(led_h);  break;
    default:          assert(0);               break;
  }
}
