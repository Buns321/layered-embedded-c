/**
 * @file motor_base.c
 * @brief 闭环电机多态基类实现。
 * @ingroup drivers
 */

#include "motor_base.h"
#include "sys_math.h"
#include <stddef.h>

bool Dev_Motor_Base_Init(motor_base_t *motor_h, const char *name, const motor_ops_t *ops,
                         controller_base_t *controller_h, bsp_encoder_t *encoder_h,
                         float counts_per_meter, float control_period_s, uint32_t max_duty) {
  if (motor_h == NULL || name == NULL || ops == NULL || ops->enable == NULL ||
      ops->disable == NULL || ops->set_power == NULL || ops->brake == NULL ||
      controller_h == NULL || !SYS_IS_POS_FINITE(control_period_s) || max_duty == 0U) {
    return false;
  }
  if (encoder_h != NULL) {
    if (!SYS_IS_POS_FINITE(counts_per_meter)) {
      return false;
    }
  } else if (ops->read_speed_mps == NULL || !SYS_IS_NONNEG_FINITE(counts_per_meter)) {
    return false;
  }

  motor_h->ops = ops;
  motor_h->name = name;
  motor_h->controller_h = controller_h;
  motor_h->encoder_h = encoder_h;
  motor_h->state = DEV_MOTOR_DISABLE;
  motor_h->target_speed_mps = 0.0f;
  motor_h->current_speed_mps = 0.0f;
  motor_h->counts_per_meter = counts_per_meter;
  motor_h->control_period_s = control_period_s;
  motor_h->max_duty = max_duty;
  motor_h->controller_status = CONTROLLER_STATUS_OK;
  return true;
}

dev_motor_state_t Dev_Motor_Base_ReadStatus(const motor_base_t *motor_h) {
  return (motor_h != NULL) ? motor_h->state : DEV_MOTOR_DISABLE;
}

float Dev_Motor_Base_ReadTargetSpeed(const motor_base_t *motor_h) {
  return (motor_h != NULL) ? motor_h->target_speed_mps : 0.0f;
}

float Dev_Motor_Base_ReadCurrentSpeed(const motor_base_t *motor_h) {
  return (motor_h != NULL) ? motor_h->current_speed_mps : 0.0f;
}

controller_status_t Dev_Motor_Base_ReadControllerStatus(const motor_base_t *motor_h) {
  return (motor_h != NULL) ? motor_h->controller_status : CONTROLLER_STATUS_ERROR;
}

void Dev_Motor_Base_SetTargetSpeed(motor_base_t *motor_h, float target_speed_mps) {
  if (motor_h == NULL) {
    return;
  }
  motor_h->target_speed_mps = SYS_ISFINITE(target_speed_mps) ? target_speed_mps : 0.0f;
}

void Dev_Motor_Base_Enable(motor_base_t *motor_h) {
  if (motor_h == NULL || motor_h->ops == NULL || motor_h->ops->enable == NULL) {
    return;
  }
  Dev_Motor_Base_Reset(motor_h);
  motor_h->ops->enable(motor_h);
  motor_h->state = DEV_MOTOR_ENABLE;
}

void Dev_Motor_Base_Disable(motor_base_t *motor_h) {
  if (motor_h == NULL || motor_h->ops == NULL || motor_h->ops->set_power == NULL ||
      motor_h->ops->disable == NULL) {
    return;
  }
  motor_h->ops->set_power(motor_h, 0.0f);
  motor_h->ops->disable(motor_h);
  motor_h->state = DEV_MOTOR_DISABLE;
  motor_h->current_speed_mps = 0.0f;
  motor_h->target_speed_mps = 0.0f;
  motor_h->controller_status = CONTROLLER_STATUS_OK;
}

void Dev_Motor_Base_Brake(motor_base_t *motor_h) {
  if (motor_h == NULL || motor_h->ops == NULL || motor_h->ops->brake == NULL ||
      motor_h->controller_h == NULL) {
    return;
  }

  motor_h->target_speed_mps = 0.0f;
  motor_h->current_speed_mps = 0.0f;
  motor_h->controller_status = CONTROLLER_STATUS_OK;

  if (motor_h->state == DEV_MOTOR_DISABLE) {
    return;
  }

  Ctrl_Base_Reset(motor_h->controller_h);
  motor_h->ops->brake(motor_h);
  motor_h->state = DEV_MOTOR_BRAKE;
}

void Dev_Motor_Base_Update(motor_base_t *motor_h) {
  if (motor_h == NULL || motor_h->ops == NULL || motor_h->ops->set_power == NULL ||
      motor_h->controller_h == NULL) {
    return;
  }
  if (motor_h->state != DEV_MOTOR_ENABLE) {
    return;
  }

  if (motor_h->encoder_h != NULL) {
    int32_t delta_counts = BSP_Encoder_GetDelta(motor_h->encoder_h);
    float speed_scale = motor_h->counts_per_meter * motor_h->control_period_s;
    if (!SYS_IS_POS_FINITE(speed_scale)) {
      motor_h->current_speed_mps = 0.0f;
      motor_h->controller_status = CONTROLLER_STATUS_ERROR;
      motor_h->ops->set_power(motor_h, 0.0f);
      return;
    }
    motor_h->current_speed_mps = (float)delta_counts / speed_scale;
  } else if (motor_h->ops->read_speed_mps != NULL) {
    float measured_mps = motor_h->ops->read_speed_mps(motor_h);
    if (!SYS_ISFINITE(measured_mps)) {
      motor_h->current_speed_mps = 0.0f;
      motor_h->controller_status = CONTROLLER_STATUS_ERROR;
      motor_h->ops->set_power(motor_h, 0.0f);
      return;
    }
    motor_h->current_speed_mps = measured_mps;
  } else {
    motor_h->current_speed_mps = 0.0f;
    motor_h->controller_status = CONTROLLER_STATUS_ERROR;
    motor_h->ops->set_power(motor_h, 0.0f);
    return;
  }

  float power_duty = 0.0f;
  controller_status_t status =
      Ctrl_Base_Update(motor_h->controller_h, motor_h->target_speed_mps, motor_h->current_speed_mps,
                       motor_h->control_period_s, &power_duty);

  if (!SYS_ISFINITE(power_duty) || status == CONTROLLER_STATUS_ERROR) {
    motor_h->controller_status = CONTROLLER_STATUS_ERROR;
    motor_h->ops->set_power(motor_h, 0.0f);
    return;
  }

  float limit_duty = (float)motor_h->max_duty;
  float clamped_duty = SYS_CLAMP(power_duty, -limit_duty, limit_duty);
  if (clamped_duty != power_duty) {
    status = CONTROLLER_STATUS_SATURATED;
  }

  motor_h->controller_status = status;
  motor_h->ops->set_power(motor_h, clamped_duty);
}

void Dev_Motor_Base_Reset(motor_base_t *motor_h) {
  if (motor_h == NULL || motor_h->ops == NULL || motor_h->ops->set_power == NULL ||
      motor_h->controller_h == NULL) {
    return;
  }
  motor_h->ops->set_power(motor_h, 0.0f);
  Ctrl_Base_Reset(motor_h->controller_h);
  if (motor_h->encoder_h != NULL) {
    BSP_Encoder_Resync(motor_h->encoder_h);
  }
  motor_h->current_speed_mps = 0.0f;
  motor_h->controller_status = CONTROLLER_STATUS_OK;
}
