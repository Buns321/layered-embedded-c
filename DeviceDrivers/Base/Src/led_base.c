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

/**
 * @brief 绑定 LED 名称与输出操作表。
 * @param led_h 调用方持有的 LED 基类存储；不可为 NULL，所有权不转移。
 * @param name 只读调试名称，生命周期必须覆盖 LED 对象；不可为 NULL。
 * @param ops 静态操作表；不可为 NULL，具体操作在使用时按需校验。
 * @pre led_h 尚未并发发布，name 与 ops 在对象使用期间保持有效。
 * @post 软件状态 is_on 被置为 DEV_LED_OFF，不改变实际 LED 输出。
 * @note 仅限初始化阶段调用；不加锁且不分配内存，重复调用会重置软件状态。
 */
void Dev_LED_Base_Init(led_base_t *led_h, const char *name, const led_ops_t *ops) {
  assert(led_h && name && ops);
  led_h->ops        = ops;
  led_h->name       = name;
  led_h->is_on      = DEV_LED_OFF;
}

/**
 * @brief 读取软件记录的 LED 开关状态。
 * @param led_h 已初始化的只读 LED 句柄；不可为 NULL，所有权不转移。
 * @retval DEV_LED_OFF 软件状态为关闭。
 * @retval DEV_LED_ON 软件状态为开启。
 * @pre led_h 已通过 Dev_LED_Base_Init 初始化。
 * @post 不访问硬件，不改变软件状态或亮度。
 * @note 状态只是最近一次成功操作后的软件镜像，不保证与外部硬件实际一致。
 */
dev_led_state_t Dev_LED_Base_ReadStatus(const led_base_t *led_h) {
  assert(led_h);
  return led_h->is_on;
}

/**
 * @brief 设置 LED 亮度码值并同步软件开关状态。
 * @param led_h 已初始化的 LED 句柄；不可为 NULL，所有权不转移。
 * @param brightness 亮度码值，范围 0..UINT16_MAX，无量纲；0 表示关闭。
 * @pre 已绑定非空 set_brightness 操作；具体驱动须支持该亮度范围。
 * @post 具体输出更新后，is_on 按 brightness 是否为 0 更新为关闭或开启。
 * @note 不支持调光的具体实现可在运行期断言；本层不加锁，须由调用方串行化。
 * @warning 输出调用失败无错误码，软件状态仍会按输入更新。
 */
void Dev_LED_Base_SetBrightness(led_base_t *led_h, uint16_t brightness) {
  assert(led_h && led_h->ops && led_h->ops->set_brightness);
  led_h->ops->set_brightness(led_h, brightness);
  led_h->is_on = brightness == 0 ? DEV_LED_OFF : DEV_LED_ON;
}

/**
 * @brief 请求 LED 全亮并同步软件状态。
 * @param led_h 已初始化的 LED 句柄；不可为 NULL，所有权不转移。
 * @pre 已绑定非空 on 操作。
 * @post 具体输出切换为开启后，is_on 被置为 DEV_LED_ON。
 * @note 本层不加锁；同一 LED 的并发操作须由调用方串行化。
 * @warning 具体输出失败无错误码，软件状态仍会置为开启。
 */
void Dev_LED_Base_TurnOn(led_base_t *led_h) {
  assert(led_h && led_h->ops && led_h->ops->on);
  led_h->ops->on(led_h);
  led_h->is_on = DEV_LED_ON;
}

/**
 * @brief 请求关闭 LED 并同步软件状态。
 * @param led_h 已初始化的 LED 句柄；不可为 NULL，所有权不转移。
 * @pre 已绑定非空 off 操作。
 * @post 具体输出关闭后，is_on 被置为 DEV_LED_OFF。
 * @note 本层不加锁；同一 LED 的并发操作须由调用方串行化。
 * @warning 具体输出失败无错误码，软件状态仍会置为关闭。
 */
void Dev_LED_Base_TurnOff(led_base_t *led_h) {
  assert(led_h && led_h->ops && led_h->ops->off);
  led_h->ops->off(led_h);
  led_h->is_on = DEV_LED_OFF;
}

/**
 * @brief 按软件状态翻转 LED 并调用对应的开或关操作。
 * @param led_h 已初始化的 LED 句柄；不可为 NULL，所有权不转移。
 * @pre is_on 必须是 DEV_LED_OFF 或 DEV_LED_ON，且对应 on/off 操作已绑定。
 * @post 软件状态翻转，具体输出按新状态更新。
 * @note 本层不加锁；与 SetBrightness、TurnOn、TurnOff 并发时结果不确定。
 * @warning 非法的 is_on 值由 assert 捕获；具体输出失败无错误码。
 */
void Dev_LED_Base_Toggle(led_base_t *led_h) {
  assert(led_h);
  switch (led_h->is_on) {
    case DEV_LED_ON:  Dev_LED_Base_TurnOff(led_h); break;
    case DEV_LED_OFF: Dev_LED_Base_TurnOn(led_h);  break;
    default:          assert(0);               break;
  }
}
