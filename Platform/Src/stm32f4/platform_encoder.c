//
// Created by Buns_ on 2026/8/24.
//

#include "../../Inc/platform_encoder.h"
#include "stm32f4xx_hal.h"
#include <assert.h>
#include "tim.h"

static TIM_HandleTypeDef *platform_to_hal_htim(Platform_TIM_Instance tim) {
  switch (tim) {
    case PLATFORM_TIM_TIM1:  return &htim1;
    case PLATFORM_TIM_TIM2:  return &htim2;
    case PLATFORM_TIM_TIM3:  return &htim3;
    case PLATFORM_TIM_TIM4:  return &htim4;
    case PLATFORM_TIM_TIM5:  return &htim5;
    // case PLATFORM_TIM_TIM6:  return &htim6;
    // case PLATFORM_TIM_TIM7:  return &htim7;
    // case PLATFORM_TIM_TIM8:  return &htim8;
    // case PLATFORM_TIM_TIM9:  return &htim9;
    // case PLATFORM_TIM_TIM10: return &htim10;
    // case PLATFORM_TIM_TIM11: return &htim11;
    // case PLATFORM_TIM_TIM12: return &htim12;
    // case PLATFORM_TIM_TIM13: return &htim13;
    // case PLATFORM_TIM_TIM14: return &htim14;

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

Platform_StatusTypeDef Platform_Encoder_Start(Platform_TIM_Instance tim, Platform_TIM_Channel channel) {
  TIM_HandleTypeDef *htim = platform_to_hal_htim(tim);
  assert(htim != NULL);
  return hal_to_platform_statustypedef(HAL_TIM_Encoder_Start(htim, platform_to_hal_channel(channel)));
}

uint32_t Platform_Encoder_GetCounter(Platform_TIM_Instance tim) {
  TIM_HandleTypeDef *htim = platform_to_hal_htim(tim);
  assert(htim != NULL);
  // return htim->Instance->CNT;
  return __HAL_TIM_GET_COUNTER(htim);
}

void Platform_Encoder_SetCounter(Platform_TIM_Instance tim, uint32_t counter) {
  TIM_HandleTypeDef *htim = platform_to_hal_htim(tim);
  assert(htim != NULL);
  __HAL_TIM_SET_COUNTER(htim, counter);
}
