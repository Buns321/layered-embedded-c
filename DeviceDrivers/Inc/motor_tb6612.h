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
 * @brief 初始化 TB6612 电机。
 * @param motor_h 对象。
 * @param name 名称。
 * @param controller_h 控制律句柄。
 * @param encoder_h 编码器句柄。
 * @param counts_per_meter 编码器每米计数。
 * @param control_period 控制周期（s）。
 * @param target_speed 初始目标速度（m/s）。
 * @param max_duty 执行器量程。
 * @param pwm_instance PWM 定时器。
 * @param pwm_channel PWM 通道。
 * @param ain1_port AIN1 端口。
 * @param ain1_pin AIN1 引脚。
 * @param ain2_port AIN2 端口。
 * @param ain2_pin AIN2 引脚。
 * @param stby_port STBY 端口。
 * @param stby_pin STBY 引脚。
 * @retval PLATFORM_STATUS_OK 初始化成功。
 * @retval PLATFORM_STATUS_ERROR PWM 启动失败。
 * @note power 的符号表示方向，绝对值范围由 max_duty 约束。
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
