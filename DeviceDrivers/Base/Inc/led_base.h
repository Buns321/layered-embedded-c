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
typedef enum {
  DEV_LED_OFF = 0u,
  DEV_LED_ON,
}Dev_LED_State;

typedef struct led_base led_base_t;

/**
 * @brief LED 具体实现的操作表。
 * @note set_brightness 的亮度范围是 0..UINT16_MAX；不支持调光的具体实现可以断言。
 */
typedef struct led_ops {
  void (*on)             (led_base_t *this);
  void (*off)            (led_base_t *this);
  void (*set_brightness) (led_base_t *this, uint16_t brightness);
}led_ops_t;

/**
 * @brief LED 抽象对象。
 */
typedef struct led_base {
  const led_ops_t *ops;
  const char      *name;
  Dev_LED_State   is_on;
}led_base_t;

/** @brief 初始化 LED 抽象层并绑定操作表。 */
void Dev_LED_Base_Init(led_base_t *this, const char *name, const led_ops_t *ops);
/** @brief 读取软件记录的开关状态。 */
Dev_LED_State Dev_LED_Base_ReadStatus(const led_base_t *this);
/**
 * @brief 设置亮度。
 * @param this LED 对象。
 * @param brightness 0..UINT16_MAX；0 表示关闭。
 * @note 同时更新软件状态 is_on。
 */
void Dev_LED_Base_SetBrightness(led_base_t *this, uint16_t brightness);
/** @brief 全亮。 */
void Dev_LED_Base_ON(led_base_t *this);
/** @brief 关闭。 */
void Dev_LED_Base_OFF(led_base_t *this);
/** @brief 按当前软件状态翻转。 */
void Dev_LED_Base_Toggle(led_base_t *this);

/** @} */

#endif //LAYERED_EMBEDDED_C_LED_BASE_H
