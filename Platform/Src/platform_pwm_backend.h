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

bool platform_pwm_backend_is_valid(platform_pwm_instance_t pwm);

platform_status_t platform_pwm_backend_start(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel
);

platform_status_t platform_pwm_backend_start_all(
  platform_pwm_instance_t pwm
);

platform_status_t platform_pwm_backend_stop(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel
);

platform_status_t platform_pwm_backend_stop_all(
  platform_pwm_instance_t pwm
);

platform_status_t platform_pwm_backend_set_compare(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel,
  uint32_t compare
);

platform_status_t platform_pwm_backend_set_duty(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel,
  uint32_t max_duty,
  uint32_t duty
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_PWM_BACKEND_H
