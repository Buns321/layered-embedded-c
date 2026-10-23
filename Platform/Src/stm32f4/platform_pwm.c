/**
 * @file platform_pwm.c
 * @brief STM32F4 HAL backend for the common PWM implementation.
 */

#include "platform_hal_internal.h"
#include "platform_pwm_backend.h"
#include "stm32f4xx_hal.h"
#include "tim.h"

static TIM_HandleTypeDef *platform_pwm_to_hal(Platform_PWM_Instance pwm) {
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

static uint32_t platform_pwm_channel_to_hal(Platform_PWM_Channel channel) {
  switch (channel) {
    case PLATFORM_PWM_CHANNEL_1: return TIM_CHANNEL_1;
    case PLATFORM_PWM_CHANNEL_2: return TIM_CHANNEL_2;
    case PLATFORM_PWM_CHANNEL_3: return TIM_CHANNEL_3;
    case PLATFORM_PWM_CHANNEL_4: return TIM_CHANNEL_4;
    default: return 0U;
  }
}

bool platform_pwm_backend_is_valid(Platform_PWM_Instance pwm) {
  return platform_pwm_to_hal(pwm) != NULL;
}

Platform_StatusTypeDef platform_pwm_backend_start(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel
) {
  TIM_HandleTypeDef *h = platform_pwm_to_hal(pwm);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_TIM_PWM_Start(
                       h,
                       platform_pwm_channel_to_hal(channel)
                     ));
}

Platform_StatusTypeDef platform_pwm_backend_start_all(
  Platform_PWM_Instance pwm
) {
  TIM_HandleTypeDef *h = platform_pwm_to_hal(pwm);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(
                       HAL_TIM_PWM_Start(h, TIM_CHANNEL_ALL)
                     );
}

Platform_StatusTypeDef platform_pwm_backend_stop(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel
) {
  TIM_HandleTypeDef *h = platform_pwm_to_hal(pwm);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_TIM_PWM_Stop(
                       h,
                       platform_pwm_channel_to_hal(channel)
                     ));
}

Platform_StatusTypeDef platform_pwm_backend_stop_all(
  Platform_PWM_Instance pwm
) {
  TIM_HandleTypeDef *h = platform_pwm_to_hal(pwm);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(
                       HAL_TIM_PWM_Stop(h, TIM_CHANNEL_ALL)
                     );
}

Platform_StatusTypeDef platform_pwm_backend_set_compare(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel,
  uint32_t compare
) {
  TIM_HandleTypeDef *h = platform_pwm_to_hal(pwm);
  if (h == NULL) {
    return Platform_INVALID_ARGUMENT;
  }
  const uint32_t autoreload = __HAL_TIM_GET_AUTORELOAD(h);
  if (compare > autoreload) {
    compare = autoreload;
  }
  __HAL_TIM_SET_COMPARE(
    h,
    platform_pwm_channel_to_hal(channel),
    compare
  );
  return Platform_OK;
}

Platform_StatusTypeDef platform_pwm_backend_set_duty(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel,
  uint32_t max_duty,
  uint32_t duty
) {
  TIM_HandleTypeDef *h = platform_pwm_to_hal(pwm);
  if (h == NULL || max_duty == 0U) {
    return Platform_INVALID_ARGUMENT;
  }
  if (duty > max_duty) {
    duty = max_duty;
  }
  const uint32_t autoreload = __HAL_TIM_GET_AUTORELOAD(h);
  const uint32_t compare = (uint32_t)(
    ((uint64_t)autoreload * duty) / max_duty
  );
  return platform_pwm_backend_set_compare(pwm, channel, compare);
}
