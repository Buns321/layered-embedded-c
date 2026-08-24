//
// Created by Buns_ on 2026/8/24.
//

#include "../Inc/motor_base.h"
#include <assert.h>

void Dev_Motor_Base_Init(motor_base_t *this, const char *name, const motor_ops_t *ops) {
  assert(this && name && ops);
  this->ops     = ops;
  this->name    = name;
  this->is_motor_enabled = DEV_MOTOR_DISABLE;
}

Dev_Motor_State Dev_Motor_Base_ReadStatus(motor_base_t *this) {
  assert(this);
  return this->is_motor_enabled;
}

int32_t Dev_Motor_Base_ReadSpeed(motor_base_t *this) {
  assert(this);
  return this->speed;
}

void Dev_Motor_Base_SetSpeed(motor_base_t *this, int32_t speed) {
  assert(this && this->ops && this->ops->set_speed);
  this->ops->set_speed(this, speed);
  this->speed = speed;
}

void Dev_Motor_Base_Enable(motor_base_t *this) {
  assert(this && this->ops && this->ops->enable);
  this->ops->enable(this);
  this->is_motor_enabled = DEV_MOTOR_ENABLE;
}

void Dev_Motor_Base_Disable(motor_base_t *this) {
  assert(this && this->ops && this->ops->disable);
  this->ops->disable(this);
  this->is_motor_enabled = DEV_MOTOR_DISABLE;
}
