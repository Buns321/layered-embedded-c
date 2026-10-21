//
// Created by Buns_ on 2026/8/24.
//

#include "platform_encoder.h"
#include "platform_hal_internal.h"
#include "stm32f4xx_hal.h"
#include "tim.h"
#include <assert.h>

static TIM_HandleTypeDef *platform_encoder_to_hal(
  Platform_Encoder_Instance encoder
) {
  switch (encoder) {
#if PLATFORM_ENABLE_ENCODER_TIM1
    case PLATFORM_ENCODER_TIM1: return &htim1;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM2
    case PLATFORM_ENCODER_TIM2: return &htim2;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM3
    case PLATFORM_ENCODER_TIM3: return &htim3;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM4
    case PLATFORM_ENCODER_TIM4: return &htim4;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM5
    case PLATFORM_ENCODER_TIM5: return &htim5;
#endif
    default: assert(0); return NULL;
  }
}

Platform_StatusTypeDef Platform_Encoder_Start(
  Platform_Encoder_Instance encoder
) {
  TIM_HandleTypeDef *htim = platform_encoder_to_hal(encoder);
  assert(htim != NULL);

  return platform_from_hal_status(
    HAL_TIM_Encoder_Start(htim, TIM_CHANNEL_ALL)
  );
}

uint32_t Platform_Encoder_GetCounter(
  Platform_Encoder_Instance encoder
) {
  TIM_HandleTypeDef *htim = platform_encoder_to_hal(encoder);
  assert(htim != NULL);
  return __HAL_TIM_GET_COUNTER(htim);
}

void Platform_Encoder_SetCounter(
  Platform_Encoder_Instance encoder,
  uint32_t counter
) {
  TIM_HandleTypeDef *htim = platform_encoder_to_hal(encoder);
  assert(htim != NULL);
  __HAL_TIM_SET_COUNTER(htim, counter);
}

uint8_t Platform_Encoder_GetCounterBits(
  Platform_Encoder_Instance encoder
) {
  switch (encoder) {
#if PLATFORM_ENABLE_ENCODER_TIM1
    case PLATFORM_ENCODER_TIM1: return 16U;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM2
    case PLATFORM_ENCODER_TIM2: return 32U;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM3
    case PLATFORM_ENCODER_TIM3: return 16U;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM4
    case PLATFORM_ENCODER_TIM4: return 16U;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM5
    case PLATFORM_ENCODER_TIM5: return 32U;
#endif
    default: assert(0); return 0U;
  }
}
