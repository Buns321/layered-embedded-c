//
// Created by Buns_ on 2026/8/24.
//

#ifndef LAYERED_EMBEDDED_C_MOTOR_BASE_H
#define LAYERED_EMBEDDED_C_MOTOR_BASE_H

#include <stdint.h>

typedef enum {
  DEV_MOTOR_DISABLE = 0U,
  DEV_MOTOR_ENABLE,
} Dev_Motor_State;

typedef struct motor_base motor_base_t;
typedef struct controller_base controller_base_t;
typedef struct encoder_base encoder_base_t;

typedef struct motor_ops {
  void (*enable)    (motor_base_t *this);
  void (*disable)   (motor_base_t *this);
  void (*set_power) (motor_base_t *this, float power); // (duty)
  void (*brake)     (motor_base_t *this);
} motor_ops_t;

typedef struct motor_base {
  const motor_ops_t *ops;
  const char        *name;
  controller_base_t *controller;
  encoder_base_t    *encoder;

  Dev_Motor_State    is_motor_enabled;

  float              target_speed;     // (m/s)
  float              current_speed;    // (m/s)
  float              counts_per_meter; // (counts/m)
  float              control_period;   // (s)
  int32_t            max_duty;         // (duty)
} motor_base_t;

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
Dev_Motor_State Dev_Motor_Base_ReadStatus       (motor_base_t *this);
float           Dev_Motor_Base_ReadTargetSpeed  (motor_base_t *this);
float           Dev_Motor_Base_ReadCurrentSpeed (motor_base_t *this);
void            Dev_Motor_Base_SetTargetSpeed   (motor_base_t *this, float target_speed);
void            Dev_Motor_Base_Enable           (motor_base_t *this);
void            Dev_Motor_Base_Disable          (motor_base_t *this);
void            Dev_Motor_Base_Update           (motor_base_t *this);
void            Dev_Motor_Base_Reset            (motor_base_t *this);

#endif //LAYERED_EMBEDDED_C_MOTOR_BASE_H
