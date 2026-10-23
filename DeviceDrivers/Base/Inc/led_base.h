//
// 由 Buns_ 创建于 2026/8/15。
//

/**
 * @file led_base.h
 * @brief LED 抽象接口：开关与调光。
 * @addtogroup drivers_base
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_LED_BASE_H
#define LAYERED_EMBEDDED_C_LED_BASE_H

#include <stdint.h>

/**
 * @brief LED 逻辑状态。
 */
typedef enum dev_led_state_e {
  DEV_LED_OFF = 0u,
  DEV_LED_ON,
} dev_led_state_t;

typedef struct led_base_s led_base_t;

/**
 * @brief LED 具体实现的操作表。
 * @note set_brightness 的亮度范围是 0..UINT16_MAX；不支持调光的具体实现可以断言。
 */
typedef struct led_ops_s {
  void (*on)             (led_base_t *led_h);
  void (*off)            (led_base_t *led_h);
  void (*set_brightness) (led_base_t *led_h, uint16_t brightness);
} led_ops_t;

/**
 * @brief LED 抽象对象。
 */
typedef struct led_base_s {
  const led_ops_t *ops;
  const char      *name;
  dev_led_state_t   is_on;
} led_base_t;

/** @brief 初始化 LED 抽象层并绑定操作表。 */
void Dev_LED_Base_Init(led_base_t *led_h, const char *name, const led_ops_t *ops);
/** @brief 读取软件记录的开关状态。 */
dev_led_state_t Dev_LED_Base_ReadStatus(const led_base_t *led_h);
/**
 * @brief 设置亮度。
 * @param led_h LED 对象。
 * @param brightness 0..UINT16_MAX；0 表示关闭。
 * @note 同时更新软件状态 is_on。
 */
void Dev_LED_Base_SetBrightness(led_base_t *led_h, uint16_t brightness);
/** @brief 全亮。 */
void Dev_LED_Base_TurnOn(led_base_t *led_h);
/** @brief 关闭。 */
void Dev_LED_Base_TurnOff(led_base_t *led_h);
/** @brief 按当前软件状态翻转。 */
void Dev_LED_Base_Toggle(led_base_t *led_h);

/** @} */

#endif //LAYERED_EMBEDDED_C_LED_BASE_H
