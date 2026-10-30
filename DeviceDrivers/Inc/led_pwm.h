//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file led_pwm.h
 * @brief PWM LED 具体实现。
 * @addtogroup drivers_concrete
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_LED_PWM_H
#define LAYERED_EMBEDDED_C_LED_PWM_H

#include "led_base.h"
#include "platform_pwm.h"
#include <stdint.h>

/**
 * @brief PWM LED 对象。
 * @note 亮度 0..UINT16_MAX 映射到 0..100% 占空比。
 */
typedef struct led_pwm_s {
  led_base_t            led_base;
  platform_pwm_instance_t pwm_instance;
  platform_pwm_channel_t  pwm_channel;
} led_pwm_t;

/**
 * @brief 初始化 PWM LED 并启动指定通道输出。
 * @param led_h 待初始化对象；非 NULL，生命周期和通道独占访问责任由调用方承担。
 * @param name 设备名称；只读字符串，由 led_base.name 保存指针而不复制。
 * @param pwm_instance PWM 定时器身份；必须是当前构建启用的实例并已完成 MX_TIMx_Init()。
 * @param pwm_channel PWM 通道身份；必须为 PLATFORM_PWM_CHANNEL_1..4，且未由其他对象独占。
 * @retval PLATFORM_STATUS_OK PWM 通道已启动，对象可用于开关和调光。
 * @retval PLATFORM_STATUS_ERROR 定时器启动失败；对象已绑定但输出不可用。
 * @retval PLATFORM_STATUS_BUSY 定时器或通道正忙；对象已绑定但输出不可用。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或通道非法；对象已绑定但启动请求被拒绝。
 * @pre 对应定时器已在 CubeMX/Core 中配置为 PWM 模式，ARR 大于 0，且调用发生在控制或显示任务启动前。
 * @pre led_h 和 name 非 NULL；本函数只对这两个空指针使用调试断言。
 * @post led_base 已绑定 PWM 操作表并记录 DEV_LED_OFF，pwm_instance/pwm_channel 已保存，且启动前已尝试写入零占空比。
 * @note 不分配内存，不复制 name；零占空比写入发生在 PWM 启动前，其失败状态被丢弃，可随后查询 Platform_PWM_GetLastError。
 * @warning 非 ISR 安全；同一 PWM 定时器或通道不得被多个 LED/驱动无协调地启动或改占空比。
 */
platform_status_t Dev_LED_PWM_Init(
  led_pwm_t             *led_h,
  const char            *name,
  platform_pwm_instance_t  pwm_instance,
  platform_pwm_channel_t   pwm_channel
);

/** @} */

#endif //LAYERED_EMBEDDED_C_LED_PWM_H
