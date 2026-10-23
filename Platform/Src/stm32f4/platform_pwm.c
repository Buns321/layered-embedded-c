//
// 由 Buns_ 创建于 2026/8/23。
//

#include "platform_pwm.h"
#include "platform_hal_internal.h"
#include "stm32f4xx_hal.h"
#include "tim.h"
#include <assert.h>

static TIM_HandleTypeDef *platform_pwm_to_hal(Platform_PWM_Instance pwm) {
  switch (pwm) {
#if PLATFORM_ENABLE_PWM_TIM1
    case PLATFORM_PWM_TIM1: return &htim1;
#endif
#if PLATFORM_ENABLE_PWM_TIM3
    case PLATFORM_PWM_TIM3: return &htim3;
#endif
    default: assert(0); return NULL;
  }
}

static uint32_t platform_pwm_channel_to_hal(Platform_PWM_Channel channel) {
  switch (channel) {
    case PLATFORM_PWM_CHANNEL_1: return TIM_CHANNEL_1;
    case PLATFORM_PWM_CHANNEL_2: return TIM_CHANNEL_2;
    case PLATFORM_PWM_CHANNEL_3: return TIM_CHANNEL_3;
    case PLATFORM_PWM_CHANNEL_4: return TIM_CHANNEL_4;
    default:                     assert(0); return 0U;
  }
}

Platform_StatusTypeDef Platform_PWM_Start(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel
) {
  TIM_HandleTypeDef *htim = platform_pwm_to_hal(pwm);
  assert(htim != NULL);

  return platform_from_hal_status(
    HAL_TIM_PWM_Start(htim, platform_pwm_channel_to_hal(channel))
  );
}

Platform_StatusTypeDef Platform_PWM_StartAll(Platform_PWM_Instance pwm) {
  TIM_HandleTypeDef *htim = platform_pwm_to_hal(pwm);
  assert(htim != NULL);

  return platform_from_hal_status(
    HAL_TIM_PWM_Start(htim, TIM_CHANNEL_ALL)
  );
}

Platform_StatusTypeDef Platform_PWM_Stop(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel
) {
  TIM_HandleTypeDef *htim = platform_pwm_to_hal(pwm);
  assert(htim != NULL);

  return platform_from_hal_status(
    HAL_TIM_PWM_Stop(htim, platform_pwm_channel_to_hal(channel))
  );
}

Platform_StatusTypeDef Platform_PWM_StopAll(Platform_PWM_Instance pwm) {
  TIM_HandleTypeDef *htim = platform_pwm_to_hal(pwm);
  assert(htim != NULL);

  return platform_from_hal_status(
    HAL_TIM_PWM_Stop(htim, TIM_CHANNEL_ALL)
  );
}

void Platform_PWM_SetCompare(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel,
  uint32_t compare
) {
  TIM_HandleTypeDef *htim = platform_pwm_to_hal(pwm);
  assert(htim != NULL);

  uint32_t auto_reload = __HAL_TIM_GET_AUTORELOAD(htim);
  if (compare > auto_reload) {
    compare = auto_reload;
  }

  __HAL_TIM_SET_COMPARE(
    htim,
    platform_pwm_channel_to_hal(channel),
    compare
  );
}

void Platform_PWM_SetDuty(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel,
  uint32_t max_duty,
  uint32_t duty
) {
  TIM_HandleTypeDef *htim = platform_pwm_to_hal(pwm);
  assert(htim != NULL && max_duty != 0U);

  if (max_duty == 0U) {
    return;
  }

  if (duty > max_duty) {
    duty = max_duty;
  }

  uint32_t auto_reload = __HAL_TIM_GET_AUTORELOAD(htim);
  uint32_t compare = (uint32_t)(
    ((uint64_t)auto_reload * (uint64_t)duty) /
    (uint64_t)max_duty
  );

  Platform_PWM_SetCompare(pwm, channel, compare);
}
