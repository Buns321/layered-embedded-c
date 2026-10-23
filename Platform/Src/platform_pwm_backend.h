/**
 * @file platform_pwm_backend.h
 * @brief Private STM32-family backend for the common PWM implementation.
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_PWM_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_PWM_BACKEND_H

#include "platform_def.h"
#include "platform_pwm.h"
#include <stdbool.h>
#include <stdint.h>

bool platform_pwm_backend_is_valid(Platform_PWM_Instance pwm);

Platform_StatusTypeDef platform_pwm_backend_start(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel
);

Platform_StatusTypeDef platform_pwm_backend_start_all(
  Platform_PWM_Instance pwm
);

Platform_StatusTypeDef platform_pwm_backend_stop(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel
);

Platform_StatusTypeDef platform_pwm_backend_stop_all(
  Platform_PWM_Instance pwm
);

Platform_StatusTypeDef platform_pwm_backend_set_compare(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel,
  uint32_t compare
);

Platform_StatusTypeDef platform_pwm_backend_set_duty(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel,
  uint32_t max_duty,
  uint32_t duty
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_PWM_BACKEND_H
