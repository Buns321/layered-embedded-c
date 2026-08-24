//
// Created by Buns_ on 2026/8/23.
//

#include "../../Inc/platform_pwm.h"
#include <assert.h>
#include "stm32f1xx_hal.h"
#include "tim.h"

static TIM_HandleTypeDef *platform_to_hal_htim(Platform_TIM_Instance tim) {
  switch (tim) {
    case PLATFORM_TIM_TIM1: return &htim1;
    // case PLATFORM_TIM_TIM2: return &htim2;
    case PLATFORM_TIM_TIM3: return &htim3;
    // case PLATFORM_TIM_TIM4: return &htim4;
    default:                assert(0); return NULL;
  }
}

static uint32_t platform_to_hal_channel(Platform_TIM_Channel channel) {
  switch (channel) {
    case PLATFORM_TIM_CHANNEL_1:    return TIM_CHANNEL_1;
    case PLATFORM_TIM_CHANNEL_2:    return TIM_CHANNEL_2;
    case PLATFORM_TIM_CHANNEL_3:    return TIM_CHANNEL_3;
    case PLATFORM_TIM_CHANNEL_4:    return TIM_CHANNEL_4;
    case PLATFORM_TIM_CHANNEL_ALL:  return TIM_CHANNEL_ALL;
    default:                        assert(0); return 0;
  }
}

static Platform_StatusTypeDef hal_to_platform_statustypedef(HAL_StatusTypeDef statustypedef) {
  switch (statustypedef) {
    case HAL_OK:      return Platform_OK;
    case HAL_ERROR:   return Platform_ERROR;
    case HAL_BUSY:    return Platform_BUSY;
    case HAL_TIMEOUT: return Platform_TIMEOUT;
    default:          assert(0); return Platform_ERROR;
  }
}

Platform_StatusTypeDef Platform_PWM_Start(Platform_TIM_Instance tim, Platform_TIM_Channel channel) {
  TIM_HandleTypeDef *htim = platform_to_hal_htim(tim);
  assert(htim != NULL);
  return  hal_to_platform_statustypedef(HAL_TIM_PWM_Start(htim, platform_to_hal_channel(channel)));
}

Platform_StatusTypeDef Platform_PWM_Stop(Platform_TIM_Instance tim, Platform_TIM_Channel channel) {
  TIM_HandleTypeDef *htim = platform_to_hal_htim(tim);
  assert(htim != NULL);
  return  hal_to_platform_statustypedef(HAL_TIM_PWM_Stop(htim, platform_to_hal_channel(channel)));
}

void Platform_PWM_SetCompare(Platform_TIM_Instance tim, Platform_TIM_Channel channel, uint32_t compare) {
  TIM_HandleTypeDef *g = platform_to_hal_htim(tim);
  assert(g != NULL);
  uint32_t cha = platform_to_hal_channel(channel);
  __HAL_TIM_SET_COMPARE(g, cha, compare);
}

void Platform_PWM_SetDuty(Platform_TIM_Instance tim, Platform_TIM_Channel channel, uint32_t max_duty, uint32_t duty) {
  TIM_HandleTypeDef *g = platform_to_hal_htim(tim);
  assert(g != NULL && max_duty);
  uint32_t arr = __HAL_TIM_GET_AUTORELOAD(g);
  uint32_t ccr = arr * duty / max_duty;
  Platform_PWM_SetCompare(tim, channel, ccr);
}
