/**
 * @file platform_pwm.c
 * @brief STM32F4 公共 PWM 实现的 HAL 后端。
 * @ingroup platform
 */

#include "platform_hal_internal.h"
#include "platform_pwm_backend.h"
#include "stm32f4xx_hal.h"
#include "tim.h"

static TIM_HandleTypeDef *platform_pwm_to_hal(platform_pwm_instance_t pwm) {
  switch (pwm) {
#if PLATFORM_ENABLE_PWM_TIM1
    case PLATFORM_PWM_TIM1: return &htim1;
#endif
#if PLATFORM_ENABLE_PWM_TIM3
    case PLATFORM_PWM_TIM3: return &htim3;
#endif
    default: return NULL;
  }
}

static uint32_t platform_pwm_channel_to_hal(platform_pwm_channel_t channel) {
  switch (channel) {
    case PLATFORM_PWM_CHANNEL_1: return TIM_CHANNEL_1;
    case PLATFORM_PWM_CHANNEL_2: return TIM_CHANNEL_2;
    case PLATFORM_PWM_CHANNEL_3: return TIM_CHANNEL_3;
    case PLATFORM_PWM_CHANNEL_4: return TIM_CHANNEL_4;
    default: return 0U;
  }
}

bool platform_pwm_backend_is_valid(platform_pwm_instance_t pwm) {
  return platform_pwm_to_hal(pwm) != NULL;
}

platform_status_t platform_pwm_backend_start(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel
) {
  TIM_HandleTypeDef *pwm_h = platform_pwm_to_hal(pwm);
  return pwm_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_TIM_PWM_Start(
                       pwm_h,
                       platform_pwm_channel_to_hal(channel)
                     ));
}

platform_status_t platform_pwm_backend_start_all(
  platform_pwm_instance_t pwm
) {
  TIM_HandleTypeDef *pwm_h = platform_pwm_to_hal(pwm);
  return pwm_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(
                       HAL_TIM_PWM_Start(pwm_h, TIM_CHANNEL_ALL)
                     );
}

platform_status_t platform_pwm_backend_stop(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel
) {
  TIM_HandleTypeDef *pwm_h = platform_pwm_to_hal(pwm);
  return pwm_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_TIM_PWM_Stop(
                       pwm_h,
                       platform_pwm_channel_to_hal(channel)
                     ));
}

platform_status_t platform_pwm_backend_stop_all(
  platform_pwm_instance_t pwm
) {
  TIM_HandleTypeDef *pwm_h = platform_pwm_to_hal(pwm);
  return pwm_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(
                       HAL_TIM_PWM_Stop(pwm_h, TIM_CHANNEL_ALL)
                     );
}

platform_status_t platform_pwm_backend_set_compare(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel,
  uint32_t compare
) {
  TIM_HandleTypeDef *pwm_h = platform_pwm_to_hal(pwm);
  if (pwm_h == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  const uint32_t autoreload = __HAL_TIM_GET_AUTORELOAD(pwm_h);
  if (compare > autoreload) {
    compare = autoreload;
  }
  __HAL_TIM_SET_COMPARE(
    pwm_h,
    platform_pwm_channel_to_hal(channel),
    compare
  );
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_pwm_backend_set_duty(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel,
  uint32_t max_duty,
  uint32_t duty
) {
  TIM_HandleTypeDef *pwm_h = platform_pwm_to_hal(pwm);
  if (pwm_h == NULL || max_duty == 0U) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (duty > max_duty) {
    duty = max_duty;
  }
  const uint32_t autoreload = __HAL_TIM_GET_AUTORELOAD(pwm_h);
  const uint32_t compare = (uint32_t)(
    ((uint64_t)autoreload * duty) / max_duty
  );
  return platform_pwm_backend_set_compare(pwm, channel, compare);
}
