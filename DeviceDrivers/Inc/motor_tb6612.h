//
// 由 Buns_ 创建于 2026/8/24。
//

/**
 * @file motor_tb6612.h
 * @brief TB6612 电机具体实现。
 * @addtogroup drivers_concrete
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_MOTOR_TB6612_H
#define LAYERED_EMBEDDED_C_MOTOR_TB6612_H

#include "motor_base.h"
#include "platform_pwm.h"
#include "platform_gpio.h"

/**
 * @brief TB6612 方向状态。
 */
typedef enum motor_tb6612_direction_e {
  DEV_TB6612_DIRECTION_REVERSE = -1,
  DEV_TB6612_DIRECTION_COAST   = 0,
  DEV_TB6612_DIRECTION_FORWARD = 1,
  DEV_TB6612_DIRECTION_BRAKE   = 2,
} motor_tb6612_direction_t;

/**
 * @brief TB6612 电机对象。
 * @note STBY 可能在板级按组共享；单电机停机不能只依赖 STBY。
 */
typedef struct motor_tb6612_s {
  motor_base_t          motor_base;
  motor_tb6612_direction_t last_direction;
  platform_pwm_instance_t pwm_instance;  platform_pwm_channel_t pwm_channel;
  platform_gpio_port_t    ain1_port;     uint16_t             ain1_pin;
  platform_gpio_port_t    ain2_port;     uint16_t             ain2_pin;
  platform_gpio_port_t    stby_port;     uint16_t             stby_pin;
} motor_tb6612_t;

/**
 * @brief 初始化 TB6612 电机、方向引脚与 PWM 通道。
 * @param motor_h 待初始化对象；非 NULL，生命周期和硬件独占访问责任由调用方承担。
 * @param name 设备名称；只读字符串，由 motor_base.name 保存指针而不复制。
 * @param controller_h 已初始化的轮级控制律；不拥有所有权，生命周期必须覆盖电机对象。
 * @param encoder_h 已初始化并启动的编码器；不拥有所有权，生命周期必须覆盖电机对象。
 * @param counts_per_meter 编码器每米计数，单位 counts/m；必须有限且大于 0。
 * @param control_period 控制周期，单位 s；必须有限且大于 0，必须与调用方的真实更新周期一致。
 * @param target_speed 初始目标速度，单位 m/s；必须有限，初值不触发执行器输出。
 * @param max_duty 执行器量程，单位 duty；必须大于 0，同时作为方向占空比绝对值上限。
 * @param pwm_instance PWM 定时器身份；必须启用并已完成 MX_TIMx_Init()。
 * @param pwm_channel PWM 通道身份；必须为 PLATFORM_PWM_CHANNEL_1..4，且未由其他对象独占。
 * @param ain1_port AIN1 GPIO 端口身份；必须启用并已完成 GPIO 初始化。
 * @param ain1_pin AIN1 单引脚掩码，不可为 0 或包含多个有效位。
 * @param ain2_port AIN2 GPIO 端口身份；必须启用并已完成 GPIO 初始化。
 * @param ain2_pin AIN2 单引脚掩码，不可为 0 或包含多个有效位。
 * @param stby_port STBY GPIO 端口身份；必须启用并已完成 GPIO 初始化。
 * @param stby_pin STBY 单引脚掩码，不可为 0 或包含多个有效位；可与其他电机共享。
 * @retval PLATFORM_STATUS_OK 电机基类、方向引脚和 PWM 已初始化，PWM 通道已启动；STBY 仍为低电平。
 * @retval PLATFORM_STATUS_ERROR PWM 启动失败；方向引脚和 STBY 已保持低电平，对象不可直接运行。
 * @retval PLATFORM_STATUS_BUSY PWM 定时器或通道正忙；方向引脚和 STBY 已保持低电平。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT PWM 实例或通道非法；方向引脚和 STBY 已保持低电平。
 * @pre motor_h、name、controller_h、encoder_h 非 NULL，控制律和编码器已初始化；仅 motor_h 空指针在此处调试断言，其余由 motor_base 初始化断言拦截。
 * @pre 所有 GPIO/PWM 外设已在 CubeMX/Core 中配置，初始化发生在控制任务使用电机之前，且无并发访问。
 * @post motor_base 已绑定 TB6612 操作表并保存控制器、编码器、速度换算和量程；AIN1/AIN2/STBY 被显式拉低，last_direction 为 COAST，随后尝试启动 PWM。
 * @note 不分配内存，不复制 name/controller/encoder；成功返回不会自动拉高 STBY，调用方必须通过 Dev_Motor_Base_Enable 显式使能。
 * @warning STBY 按板级接线可能共享；单电机初始化为低会同时封锁同组输出。非 ISR 安全，不得与其他控制访问并发。
 */
platform_status_t Dev_Motor_TB6612_Init(
  motor_tb6612_t        *motor_h,
  const char            *name,
  controller_base_t     *controller_h,
  encoder_base_t        *encoder_h,
  float                  counts_per_meter,
  float                  control_period,
  float                  target_speed,
  int32_t                max_duty,
  platform_pwm_instance_t  pwm_instance,
  platform_pwm_channel_t   pwm_channel,
  platform_gpio_port_t     ain1_port, uint16_t ain1_pin,
  platform_gpio_port_t     ain2_port, uint16_t ain2_pin,
  platform_gpio_port_t     stby_port, uint16_t stby_pin
);

/** @} */

#endif //LAYERED_EMBEDDED_C_MOTOR_TB6612_H
