//
// 由 Buns_ 创建于 2026/8/24。
//

/**
 * @file motor_base.h
 * @brief 闭环电机抽象：目标速度、编码器、控制律与执行器。
 * @addtogroup drivers_base
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_MOTOR_BASE_H
#define LAYERED_EMBEDDED_C_MOTOR_BASE_H

#include <stdint.h>
#include "controller_base.h"

/**
 * @brief 电机使能状态。
 */
typedef enum {
  DEV_MOTOR_DISABLE = 0U,
  DEV_MOTOR_ENABLE,
  DEV_MOTOR_BRAKE,
} Dev_Motor_State;

typedef struct motor_base motor_base_t;
typedef struct encoder_base encoder_base_t;

/**
 * @brief 电机具体实现的操作表。
 * @note set_power 的参数是带符号占空比，范围由 max_duty 约束。
 */
typedef struct motor_ops {
  void (*enable)    (motor_base_t *this);
  void (*disable)   (motor_base_t *this);
  void (*set_power) (motor_base_t *this, float power); // (duty)
  void (*brake)     (motor_base_t *this);
} motor_ops_t;

/**
 * @brief 闭环电机对象。
 * @note target_speed / current_speed 单位 m/s。
 * @note counts_per_meter 是编码器每米计数；control_period 单位 s。
 * @note max_duty 是执行器量程；controller_status 保存最近一次控制律状态。
 */
typedef struct motor_base {
  const motor_ops_t *ops;
  const char        *name;
  controller_base_t *controller;
  encoder_base_t    *encoder;

  Dev_Motor_State    state;            // 禁用 / 闭环使能 / 制动保持

  float              target_speed;     // (m/s)
  float              current_speed;    // (m/s)
  float              counts_per_meter; // (counts/m)
  float              control_period;   // (s)
  int32_t            max_duty;         // (duty)
  controller_status_t controller_status;
} motor_base_t;

/** @brief 初始化电机抽象层并注入控制律与编码器。 */
void Dev_Motor_Base_Init(
  motor_base_t      *this,
  const char        *name,
  const motor_ops_t *ops,
  controller_base_t *controller,
  encoder_base_t    *encoder,
  float              counts_per_meter,
  float              control_period,
  float              target_speed,
  int32_t            max_duty
);
/** @brief 读取当前运行状态。 */
Dev_Motor_State Dev_Motor_Base_ReadStatus       (motor_base_t *this);
/** @brief 读取目标速度（m/s）。 */
float           Dev_Motor_Base_ReadTargetSpeed  (motor_base_t *this);
/** @brief 读取最近一次计算出的当前速度（m/s）。 */
float           Dev_Motor_Base_ReadCurrentSpeed (motor_base_t *this);
/** @brief 读取最近一次控制律状态。 */
controller_status_t Dev_Motor_Base_ReadControllerStatus(motor_base_t *this);
/** @brief 设置目标速度（m/s）；只保存目标，不产生输出。 */
void            Dev_Motor_Base_SetTargetSpeed   (motor_base_t *this, float target_speed);
/** @brief 使能电机；内部会先执行 Reset。 */
void            Dev_Motor_Base_Enable           (motor_base_t *this);
/** @brief 禁用电机并清除目标。 */
void            Dev_Motor_Base_Disable          (motor_base_t *this);
/**
 * @brief 使用具体驱动的制动能力，并保持制动状态。
 * @note 制动后 Update 不再输出，必须重新 Enable 才能恢复闭环控制。
 */
void            Dev_Motor_Base_Brake            (motor_base_t *this);
/**
 * @brief 运行一次闭环更新。
 * @note 每个控制周期调用一次；内部读取编码器 delta、运行控制律并下发占空比。
 * @warning 非 ENABLE 状态直接返回，不读取编码器。
 */
void            Dev_Motor_Base_Update           (motor_base_t *this);
/**
 * @brief 复位控制律并重新对齐编码器基线。
 * @note 不会改变目标速度。
 */
void            Dev_Motor_Base_Reset            (motor_base_t *this);

/** @} */

#endif //LAYERED_EMBEDDED_C_MOTOR_BASE_H
