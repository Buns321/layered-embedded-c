/**
 * @file platform_pwm_common.c
 * @brief Family-independent PWM validation and error snapshot.
 */

#include "platform_error_internal.h"
#include "platform_pwm_backend.h"

static volatile uint32_t pwm_error[2];

static uint32_t platform_pwm_error_index(Platform_PWM_Instance pwm) {
  return pwm == PLATFORM_PWM_TIM1 ? 0U : 1U;
}

static Platform_StatusTypeDef platform_pwm_finish(
  Platform_PWM_Instance pwm,
  Platform_StatusTypeDef status
) {
  if (status != Platform_OK) {
    platform_error_record(
      &pwm_error[platform_pwm_error_index(pwm)],
      platform_error_from_status(status)
    );
  }
  return status;
}

static bool platform_pwm_channel_valid(Platform_PWM_Channel channel) {
  return (uint32_t)channel <= (uint32_t)PLATFORM_PWM_CHANNEL_4;
}

static Platform_StatusTypeDef platform_pwm_invalid(
  Platform_PWM_Instance pwm,
  bool instance_valid
) {
  if (instance_valid) {
    platform_error_record(
      &pwm_error[platform_pwm_error_index(pwm)],
      PLATFORM_COMMON_ERROR_PARAMETER
    );
  }
  return Platform_INVALID_ARGUMENT;
}

Platform_StatusTypeDef Platform_PWM_Start(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel
) {
  const bool instance_valid = platform_pwm_backend_is_valid(pwm);
  if (instance_valid) {
    platform_error_reset(&pwm_error[platform_pwm_error_index(pwm)]);
  }
  if (!instance_valid || !platform_pwm_channel_valid(channel)) {
    return platform_pwm_invalid(pwm, instance_valid);
  }
  return platform_pwm_finish(
    pwm,
    platform_pwm_backend_start(pwm, channel)
  );
}

Platform_StatusTypeDef Platform_PWM_StartAll(Platform_PWM_Instance pwm) {
  const bool instance_valid = platform_pwm_backend_is_valid(pwm);
  if (instance_valid) {
    platform_error_reset(&pwm_error[platform_pwm_error_index(pwm)]);
  }
  if (!instance_valid) {
    return platform_pwm_invalid(pwm, false);
  }
  return platform_pwm_finish(pwm, platform_pwm_backend_start_all(pwm));
}

Platform_StatusTypeDef Platform_PWM_Stop(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel
) {
  const bool instance_valid = platform_pwm_backend_is_valid(pwm);
  if (instance_valid) {
    platform_error_reset(&pwm_error[platform_pwm_error_index(pwm)]);
  }
  if (!instance_valid || !platform_pwm_channel_valid(channel)) {
    return platform_pwm_invalid(pwm, instance_valid);
  }
  return platform_pwm_finish(
    pwm,
    platform_pwm_backend_stop(pwm, channel)
  );
}

Platform_StatusTypeDef Platform_PWM_StopAll(Platform_PWM_Instance pwm) {
  const bool instance_valid = platform_pwm_backend_is_valid(pwm);
  if (instance_valid) {
    platform_error_reset(&pwm_error[platform_pwm_error_index(pwm)]);
  }
  if (!instance_valid) {
    return platform_pwm_invalid(pwm, false);
  }
  return platform_pwm_finish(pwm, platform_pwm_backend_stop_all(pwm));
}

Platform_StatusTypeDef Platform_PWM_SetCompare(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel,
  uint32_t compare
) {
  const bool instance_valid = platform_pwm_backend_is_valid(pwm);
  if (instance_valid) {
    platform_error_reset(&pwm_error[platform_pwm_error_index(pwm)]);
  }
  if (!instance_valid || !platform_pwm_channel_valid(channel)) {
    return platform_pwm_invalid(pwm, instance_valid);
  }
  return platform_pwm_finish(
    pwm,
    platform_pwm_backend_set_compare(pwm, channel, compare)
  );
}

Platform_StatusTypeDef Platform_PWM_SetDuty(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel,
  uint32_t max_duty,
  uint32_t duty
) {
  const bool instance_valid = platform_pwm_backend_is_valid(pwm);
  if (instance_valid) {
    platform_error_reset(&pwm_error[platform_pwm_error_index(pwm)]);
  }
  if (
    !instance_valid ||
    !platform_pwm_channel_valid(channel) ||
    max_duty == 0U
  ) {
    return platform_pwm_invalid(pwm, instance_valid);
  }
  return platform_pwm_finish(
    pwm,
    platform_pwm_backend_set_duty(pwm, channel, max_duty, duty)
  );
}

uint32_t Platform_PWM_GetLastError(Platform_PWM_Instance pwm) {
  if (!platform_pwm_backend_is_valid(pwm)) {
    return PLATFORM_COMMON_ERROR_PARAMETER;
  }
  return pwm_error[platform_pwm_error_index(pwm)];
}
