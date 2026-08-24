//
// Created by Buns_ on 2026/8/24.
//

#ifndef LAYERED_EMBEDDED_C_MOTOR_BASE_H
#define LAYERED_EMBEDDED_C_MOTOR_BASE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  DEV_MOTOR_DISABLE = 0U,
  DEV_MOTOR_ENABLE,
} Dev_Motor_State;

typedef enum {
  DEV_MOTOR_PWM_DISABLE = 0U,
  DEV_MOTOR_PWM_ENABLE,
} Dev_Motor_PWM_State;

typedef struct motor_base motor_base_t;

typedef struct motor_ops {
  void (*enable)    (motor_base_t *this);
  void (*disable)   (motor_base_t *this);
  void (*set_speed) (motor_base_t *this, int32_t speed);
  void (*brake)     (motor_base_t *this);
} motor_ops_t;

typedef struct motor_base {
  const motor_ops_t   *ops;
  const char          *name;
  Dev_Motor_State     is_motor_enabled;
  Dev_Motor_PWM_State is_pwm_enabled;
  int32_t             speed;
  int32_t             max_speed;
} motor_base_t;

void Dev_Motor_Base_Init(motor_base_t *this, const char *name, const motor_ops_t *ops);
Dev_Motor_State Dev_Motor_Base_ReadStatus(motor_base_t *this);
int32_t Dev_Motor_Base_ReadSpeed(motor_base_t *this);
void Dev_Motor_Base_SetSpeed(motor_base_t *this, int32_t speed);
void Dev_Motor_Base_Enable(motor_base_t *this);
void Dev_Motor_Base_Disable(motor_base_t *this);

#endif //LAYERED_EMBEDDED_C_MOTOR_BASE_H
