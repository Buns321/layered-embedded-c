/**
 * @file motor_base.h
 * @brief 闭环电机多态基类：编码器/反馈测速、SISO 控制律调度与状态机。
 * @addtogroup drivers
 * @{
 */

#ifndef LAYERED_CAR_MOTOR_BASE_H
#define LAYERED_CAR_MOTOR_BASE_H

#include "bsp_encoder.h"
#include "controller_base.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 电机运行状态。
 */
typedef enum dev_motor_state_e {
  DEV_MOTOR_DISABLE = 0U,
  DEV_MOTOR_ENABLE,
  DEV_MOTOR_BRAKE
} dev_motor_state_t;

typedef struct motor_base_s motor_base_t;

/**
 * @brief 电机执行器虚表（power_duty 单位 duty，范围 [-max_duty, +max_duty]）。
 */
typedef struct motor_ops_s {
  void (*enable)(motor_base_t *motor_h);
  void (*disable)(motor_base_t *motor_h);
  void (*set_power)(motor_base_t *motor_h, float power_duty);
  void (*brake)(motor_base_t *motor_h);
  float (*read_speed_mps)(motor_base_t *motor_h); /* 可选：无外部编码器的总线电机使用 */
} motor_ops_t;

/**
 * @brief 闭环电机多态基类；须作为具体电机结构体首成员。
 */
typedef struct motor_base_s {
  const motor_ops_t *ops;
  const char *name;
  controller_base_t *controller_h;
  bsp_encoder_t *encoder_h;
  dev_motor_state_t state;
  float target_speed_mps;
  float current_speed_mps;
  float counts_per_meter;
  float control_period_s;
  uint32_t max_duty;
  controller_status_t controller_status;
} motor_base_t;

/**
 * @brief 绑定电机虚表、控制律与测速编码器（encoder_h 可为 NULL 若 ops 提供 read_speed_mps）。
 */
bool Dev_Motor_Base_Init(motor_base_t *motor_h, const char *name, const motor_ops_t *ops,
                         controller_base_t *controller_h, bsp_encoder_t *encoder_h,
                         float counts_per_meter, float control_period_s, uint32_t max_duty);

/** @brief 查询电机当前运行状态。 */
dev_motor_state_t Dev_Motor_Base_ReadStatus(const motor_base_t *motor_h);

/** @brief 查询最近设置的目标速度，单位 m/s。 */
float Dev_Motor_Base_ReadTargetSpeed(const motor_base_t *motor_h);

/** @brief 查询最近一次闭环更新计算出的实测速度，单位 m/s。 */
float Dev_Motor_Base_ReadCurrentSpeed(const motor_base_t *motor_h);

/** @brief 查询最近一次闭环更新的控制律状态。 */
controller_status_t Dev_Motor_Base_ReadControllerStatus(const motor_base_t *motor_h);

/** @brief 设置目标线速度，单位 m/s（非有限值按 0 处理）。 */
void Dev_Motor_Base_SetTargetSpeed(motor_base_t *motor_h, float target_speed_mps);

/** @brief 复位闭环状态并使能执行器。 */
void Dev_Motor_Base_Enable(motor_base_t *motor_h);

/** @brief 先下发零占空比，再禁用执行器并清零速度目标。 */
void Dev_Motor_Base_Disable(motor_base_t *motor_h);

/** @brief 清零目标与当前速度并请求执行器制动。 */
void Dev_Motor_Base_Brake(motor_base_t *motor_h);

/** @brief 在 ENABLE 状态下采样速度、运行控制律并更新执行器占空比（每周期单次调用）。 */
void Dev_Motor_Base_Update(motor_base_t *motor_h);

/** @brief 归零输出、复位控制律并重对齐编码器速度基线。 */
void Dev_Motor_Base_Reset(motor_base_t *motor_h);

/** @} */

#endif /* LAYERED_CAR_MOTOR_BASE_H */
