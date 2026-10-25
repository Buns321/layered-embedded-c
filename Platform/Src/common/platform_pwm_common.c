/**
 * @file platform_pwm_common.c
 * @brief 与芯片系列无关的 PWM 参数校验与错误快照。
 * @ingroup platform
 */

#include "platform_error_internal.h"
#include "platform_pwm_backend.h"

static volatile uint32_t pwm_error[PLATFORM_PWM_INSTANCE_COUNT];

static uint32_t platform_pwm_error_index(platform_pwm_instance_t pwm) {
  return (uint32_t)pwm;
}

static platform_status_t platform_pwm_finish(
  platform_pwm_instance_t pwm,
  platform_status_t status
) {
  if (status != PLATFORM_STATUS_OK) {
    platform_error_record(
      &pwm_error[platform_pwm_error_index(pwm)],
      platform_error_from_status(status)
    );
  }
  return status;
}

static bool platform_pwm_channel_valid(platform_pwm_channel_t channel) {
  return (uint32_t)channel <= (uint32_t)PLATFORM_PWM_CHANNEL_4;
}

static platform_status_t platform_pwm_invalid(
  platform_pwm_instance_t pwm,
  bool instance_valid
) {
  if (instance_valid) {
    platform_error_record(
      &pwm_error[platform_pwm_error_index(pwm)],
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
  }
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

platform_status_t Platform_PWM_Start(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel
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

platform_status_t Platform_PWM_StartAll(platform_pwm_instance_t pwm) {
  const bool instance_valid = platform_pwm_backend_is_valid(pwm);
  if (instance_valid) {
    platform_error_reset(&pwm_error[platform_pwm_error_index(pwm)]);
  }
  if (!instance_valid) {
    return platform_pwm_invalid(pwm, false);
  }
  return platform_pwm_finish(pwm, platform_pwm_backend_start_all(pwm));
}

platform_status_t Platform_PWM_Stop(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel
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

platform_status_t Platform_PWM_StopAll(platform_pwm_instance_t pwm) {
  const bool instance_valid = platform_pwm_backend_is_valid(pwm);
  if (instance_valid) {
    platform_error_reset(&pwm_error[platform_pwm_error_index(pwm)]);
  }
  if (!instance_valid) {
    return platform_pwm_invalid(pwm, false);
  }
  return platform_pwm_finish(pwm, platform_pwm_backend_stop_all(pwm));
}

platform_status_t Platform_PWM_SetCompare(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel,
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

platform_status_t Platform_PWM_SetDuty(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel,
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

uint32_t Platform_PWM_GetLastError(platform_pwm_instance_t pwm) {
  if (!platform_pwm_backend_is_valid(pwm)) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return pwm_error[platform_pwm_error_index(pwm)];
}
