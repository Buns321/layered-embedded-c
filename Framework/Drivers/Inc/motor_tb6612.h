/**
 * @file motor_tb6612.h
 * @brief TB6612 双极性直流电机驱动（单配置结构体初始化与换向先撤占空比保护）。
 * @addtogroup drivers
 * @{
 */

#ifndef LAYERED_CAR_MOTOR_TB6612_H
#define LAYERED_CAR_MOTOR_TB6612_H

#include "bsp_def.h"
#include "bsp_gpio.h"
#include "bsp_pwm.h"
#include "motor_base.h"

/**
 * @brief TB6612 H 桥当前逻辑方向。
 */
typedef enum motor_tb6612_dir_e {
  DEV_TB6612_DIR_REVERSE = -1,
  DEV_TB6612_DIR_COAST = 0,
  DEV_TB6612_DIR_FORWARD = 1,
  DEV_TB6612_DIR_BRAKE = 2
} motor_tb6612_dir_t;

/**
 * @brief TB6612 电机初始化配置；stby 允许共享或置 {NULL, 0}。
 */
typedef struct motor_tb6612_cfg_s {
  const char *name;
  bsp_pwm_t pwm;
  bsp_gpio_t in1;
  bsp_gpio_t in2;
  bsp_gpio_t stby;
  bsp_encoder_t *encoder;
  controller_base_t *controller;
  float counts_per_meter; /* > 0 */
  float control_period_s; /* > 0 */
} motor_tb6612_cfg_t;

/**
 * @brief TB6612 电机静态对象（base 必须为首成员）。
 */
typedef struct motor_tb6612_s {
  motor_base_t base;
  motor_tb6612_dir_t last_direction;
  bsp_pwm_t pwm;
  bsp_gpio_t in1;
  bsp_gpio_t in2;
  bsp_gpio_t stby;
} motor_tb6612_t;

/**
 * @brief 初始化 TB6612 电机并启动 PWM 通道（初始化后 STBY 保持低电平，须显式 Enable）。
 */
bsp_status_t Dev_Motor_TB6612_Init(motor_tb6612_t *motor, const motor_tb6612_cfg_t *cfg);

/**
 * @brief 紧急断电停机：直接清零 PWM 并拉低 IN1/IN2/STBY（可用于 Fault/栈溢出钩子）。
 */
void Dev_Motor_TB6612_EmergencyHalt(motor_tb6612_t *motor);

/** @} */

#endif /* LAYERED_CAR_MOTOR_TB6612_H */
