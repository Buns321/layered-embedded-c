/**
 * @file platform_encoder.c
 * @brief STM32F1 HAL backend for the common encoder implementation.
 */

#include "platform_encoder_backend.h"
#include "platform_hal_internal.h"
#include "stm32f1xx_hal.h"
#include "tim.h"

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
    default: return NULL;
  }
}

bool platform_encoder_backend_is_valid(
  Platform_Encoder_Instance encoder
) {
  return platform_encoder_to_hal(encoder) != NULL;
}

Platform_StatusTypeDef platform_encoder_backend_start(
  Platform_Encoder_Instance encoder
) {
  TIM_HandleTypeDef *h = platform_encoder_to_hal(encoder);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(
                       HAL_TIM_Encoder_Start(h, TIM_CHANNEL_ALL)
                     );
}

Platform_StatusTypeDef platform_encoder_backend_stop(
  Platform_Encoder_Instance encoder
) {
  TIM_HandleTypeDef *h = platform_encoder_to_hal(encoder);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(
                       HAL_TIM_Encoder_Stop(h, TIM_CHANNEL_ALL)
                     );
}

Platform_StatusTypeDef platform_encoder_backend_get_counter(
  Platform_Encoder_Instance encoder,
  uint32_t *counter
) {
  TIM_HandleTypeDef *h = platform_encoder_to_hal(encoder);
  if (h == NULL || counter == NULL) {
    return Platform_INVALID_ARGUMENT;
  }
  *counter = __HAL_TIM_GET_COUNTER(h);
  return Platform_OK;
}

Platform_StatusTypeDef platform_encoder_backend_set_counter(
  Platform_Encoder_Instance encoder,
  uint32_t counter
) {
  TIM_HandleTypeDef *h = platform_encoder_to_hal(encoder);
  if (h == NULL) {
    return Platform_INVALID_ARGUMENT;
  }
  __HAL_TIM_SET_COUNTER(h, counter);
  return Platform_OK;
}

Platform_StatusTypeDef platform_encoder_backend_get_counter_bits(
  Platform_Encoder_Instance encoder,
  uint8_t *counter_bits
) {
  if (counter_bits == NULL) {
    return Platform_INVALID_ARGUMENT;
  }
  switch (encoder) {
#if PLATFORM_ENABLE_ENCODER_TIM1
    case PLATFORM_ENCODER_TIM1: *counter_bits = 16U; return Platform_OK;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM2
    case PLATFORM_ENCODER_TIM2: *counter_bits = 16U; return Platform_OK;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM3
    case PLATFORM_ENCODER_TIM3: *counter_bits = 16U; return Platform_OK;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM4
    case PLATFORM_ENCODER_TIM4: *counter_bits = 16U; return Platform_OK;
#endif
    default: return Platform_INVALID_ARGUMENT;
  }
}
